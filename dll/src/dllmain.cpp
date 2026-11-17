// Olympian Planet Coords (DLL) - paso 2: coordenadas en vivo para el overlay
// Olympian Planet Coords (DLL) - step 2: live coordinates for the overlay
//
// Olympian Planet Coords — Copyright © 2026 MightyZeus · Licencia GPL-3.0
// Apoya el proyecto: https://paypal.me/ImMightyZeus · Repositorio: https://github.com/ImMightyZeus/Olympian-Planet-Coords
//
// Agradecimientos:
// A mi querida esposa y mi amada familia, por permitirme seguir siendo un niño aun en mi adultez.
// Pangro Daer Barun D'Gross, por seguirme corrompiendo con videojuegos.
// Fenrir, porque aunque ya casi no juega, aún se presta a conversar sobre mis aventuras y desventuras.
//
// Proyecto hecho por fans, sin relacion con Hello Games. Se desarrolla con la ayuda de una IA (Claude);
// la direccion del proyecto y las pruebas en el juego real son trabajo humano.
//
// Se instala como "winmm.dll" junto a NMS.exe. Windows lo carga en lugar del winmm.dll del sistema;
// este DLL carga el verdadero (System32) y le reenvia todas las llamadas, asi que el juego funciona
// igual. Ademas:
//   1. Comprueba que la version de NMS.exe sea la esperada (si no, no hace nada mas).
//   2. Busca en NMS.exe las funciones del juego (patrones de NMS.py) y lo anota en el log.
//   3. Engancha cGcApplication::Update: en cada frame LEE (nunca escribe) la posicion del jugador
//      y el planeta, y calcula las coordenadas igual que la version Python v0.0.3.
//   4. Sirve esas coordenadas en http://127.0.0.1:5000/OlympianPlanetCoords/ para el overlay.

#include <windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <atomic>
#include <memory>

#include "MinHook.h"
#include "core.hpp"
#include "game_reader.hpp"
#include "http_server.hpp"
#include "nms_sigs.h"
#include "routes.hpp"
#include "winmm_exports.h"

#define OLYMPIAN_VERSION "0.1.0-dll.2"
constexpr int kHttpPort = 5000;

extern "C" {
// Tabla que usan los saltos de winmm_stubs.S (un puntero por export, en el mismo orden).
void* g_winmm_real[WINMM_EXPORT_COUNT];
}

namespace {

HMODULE g_self = nullptr;
HMODULE g_real_winmm = nullptr;
FILE* g_log = nullptr;
CRITICAL_SECTION g_log_lock;

UINT WINAPI MissingExport() { return 0; }  // export que no existe en este Windows

// ---- estado del mod --------------------------------------------------------------------------
using UpdateFn = void (*)(void*);
UpdateFn g_orig_update = nullptr;
std::unique_ptr<olympian::Core> g_core;
std::unique_ptr<olympian::HttpServer> g_http;
olympian::Json g_debug_extra = olympian::Json::object();
std::atomic<unsigned long long> g_frames{0};
std::atomic<unsigned long long> g_frames_with_game{0};
std::atomic<uintptr_t> g_app{0};

std::string Utf8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

// %LOCALAPPDATA%\OlympianPlanetCoords\datos.json: el mismo archivo que usa la version Python.
std::wstring DataDir() {
    wchar_t local[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH);
    if (!n || n >= MAX_PATH) return L"";
    return std::wstring(local, n) + L"\\OlympianPlanetCoords";
}

std::string SystemLang() {
    return (GetUserDefaultUILanguage() & 0x3FF) == 0x0A ? "es" : "en";
}

// Se ejecuta en el hilo del juego, una vez por frame. Primero el juego hace lo suyo; despues se lee.
void HookedUpdate(void* app) {
    g_orig_update(app);
    ++g_frames;
    if (!g_core) return;
    try {
        g_app = reinterpret_cast<uintptr_t>(app);
        olympian::GameView view;
        bool ok = olympian::ReadGameView(reinterpret_cast<uintptr_t>(app), &view);
        if (ok) ++g_frames_with_game;
        g_core->Tick(ok ? &view : nullptr, static_cast<double>(GetTickCount64()) / 1000.0);
    } catch (...) {
        // Nunca dejar que un error del mod llegue al juego.
    }
}

std::wstring ExeDir() {
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring s(path, n);
    size_t p = s.find_last_of(L"\\/");
    return p == std::wstring::npos ? L"." : s.substr(0, p);
}

void Log(const char* fmt, ...) {
    if (!g_log) return;
    EnterCriticalSection(&g_log_lock);
    SYSTEMTIME t;
    GetLocalTime(&t);
    fprintf(g_log, "%02d:%02d:%02d.%03d ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fputc('\n', g_log);
    fflush(g_log);
    LeaveCriticalSection(&g_log_lock);
}

bool LoadRealWinmm() {
    wchar_t sys[MAX_PATH];
    UINT n = GetSystemDirectoryW(sys, MAX_PATH);
    if (n == 0 || n >= MAX_PATH - 12) return false;
    std::wstring path = std::wstring(sys, n) + L"\\winmm.dll";
    g_real_winmm = LoadLibraryW(path.c_str());
    if (!g_real_winmm) return false;
    for (int i = 0; i < WINMM_EXPORT_COUNT; ++i) {
        FARPROC f = GetProcAddress(g_real_winmm, kWinmmExports[i]);
        g_winmm_real[i] = f ? reinterpret_cast<void*>(f) : reinterpret_cast<void*>(&MissingExport);
    }
    return true;
}

// Version del archivo NMS.exe (recurso VS_VERSION_INFO), p. ej. "185034".
std::string ExeFileVersion() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    DWORD dummy = 0, size = GetFileVersionInfoSizeW(path, &dummy);
    if (!size) return "";
    std::vector<BYTE> buf(size);
    if (!GetFileVersionInfoW(path, 0, size, buf.data())) return "";
    struct LangCp { WORD lang, cp; }* tr = nullptr;
    UINT len = 0;
    if (!VerQueryValueW(buf.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<void**>(&tr), &len) || len < 4)
        return "";
    wchar_t key[64];
    swprintf(key, 64, L"\\StringFileInfo\\%04x%04x\\FileVersion", tr->lang, tr->cp);
    wchar_t* val = nullptr;
    if (!VerQueryValueW(buf.data(), key, reinterpret_cast<void**>(&val), &len) || !val) return "";
    char out[64];
    WideCharToMultiByte(CP_UTF8, 0, val, -1, out, sizeof(out), nullptr, nullptr);
    return out;
}

struct Pattern {
    std::vector<int> bytes;  // -1 = comodin "?"
};

Pattern ParsePattern(const char* s) {
    Pattern p;
    while (*s) {
        while (*s == ' ') ++s;
        if (!*s) break;
        if (*s == '?') {
            p.bytes.push_back(-1);
            while (*s == '?') ++s;
        } else {
            p.bytes.push_back(static_cast<int>(strtoul(s, const_cast<char**>(&s), 16)));
        }
    }
    return p;
}

// Busca el patron en la seccion .text de NMS.exe ya cargado en memoria.
// Devuelve la direccion de la primera coincidencia (o 0) y el numero total de coincidencias.
uintptr_t FindPattern(const Pattern& pat, int* count) {
    auto base = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    auto sec = IMAGE_FIRST_SECTION(nt);
    uintptr_t first = 0;
    *count = 0;
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        if (memcmp(sec->Name, ".text", 5) != 0) continue;
        uint8_t* start = base + sec->VirtualAddress;
        size_t size = sec->Misc.VirtualSize, n = pat.bytes.size();
        for (size_t off = 0; off + n <= size; ++off) {
            size_t k = 0;
            for (; k < n; ++k) {
                int b = pat.bytes[k];
                if (b >= 0 && start[off + k] != b) break;
            }
            if (k == n) {
                if (!first) first = reinterpret_cast<uintptr_t>(start + off);
                ++*count;
            }
        }
    }
    return first;
}

DWORD WINAPI Worker(LPVOID) {
    // Log junto al juego; si Windows no deja escribir ahi (sin admin), en %LOCALAPPDATA%.
    std::wstring dir = ExeDir() + L"\\Olympian";
    CreateDirectoryW(dir.c_str(), nullptr);
    g_log = _wfopen((dir + L"\\olympian.log").c_str(), L"w");
    if (!g_log) {
        wchar_t local[MAX_PATH];
        DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH);
        if (n && n < MAX_PATH) {
            dir = std::wstring(local, n) + L"\\OlympianPlanetCoords";
            CreateDirectoryW(dir.c_str(), nullptr);
            g_log = _wfopen((dir + L"\\olympian.log").c_str(), L"w");
        }
    }
    Log("Olympian Planet Coords %s - MightyZeus (https://github.com/ImMightyZeus)", OLYMPIAN_VERSION);
    Log("winmm real cargado desde System32: %s", g_real_winmm ? "si" : "NO");

    std::string ver = ExeFileVersion();
    Log("NMS.exe version=%s  esperada=%s", ver.c_str(), NMS_EXPECTED_BUILD);
    if (ver != NMS_EXPECTED_BUILD) {
        Log("VERSION DISTINTA: el mod queda desactivado (el juego funciona normal). Hace falta "
            "actualizar Olympian para esta version.");
        return 0;
    }
    uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    // El juego puede seguir preparando su codigo cuando Windows carga este DLL: si falta alguna
    // funcion, se reintenta cada segundo durante 90 s antes de rendirse.
    int ok = 0;
    for (int attempt = 0; attempt < 90; ++attempt) {
        ok = 0;
        bool log_now = attempt == 0 || attempt == 89;
        for (int i = 0; i < FN_COUNT; ++i) {
            int count = 0;
            uintptr_t addr = FindPattern(ParsePattern(kNmsSigs[i].pattern), &count);
            if (addr && count == 1) ++ok;
            if (log_now)
                Log("%-40s %s  rva=0x%llx  coincidencias=%d", kNmsSigs[i].name, addr ? "OK " : "NO ",
                    static_cast<unsigned long long>(addr ? addr - base : 0), count);
        }
        if (ok == FN_COUNT) {
            if (attempt > 0) Log("Funciones encontradas en el intento %d.", attempt + 1);
            break;
        }
        if (attempt == 0) Log("Faltan funciones (%d/%d); se reintenta cada segundo...", ok, static_cast<int>(FN_COUNT));
        Sleep(1000);
    }
    if (ok != FN_COUNT) {
        Log("No se encontraron todas las funciones: el mod queda desactivado (el juego funciona normal).");
        return 0;
    }

    // Datos (mismo datos.json que la version Python) y servidor para el overlay.
    std::wstring ddir = DataDir();
    if (ddir.empty()) {
        Log("No se encontro %%LOCALAPPDATA%%: el mod queda desactivado.");
        return 0;
    }
    CreateDirectoryW(ddir.c_str(), nullptr);
    std::string data_path = Utf8(ddir + L"\\datos.json");
    g_core = std::make_unique<olympian::Core>(data_path, SystemLang());
    Log("Datos: %s  idioma=%s", data_path.c_str(), g_core->Lang().c_str());

    g_debug_extra = olympian::Json{{"dll", {{"version", OLYMPIAN_VERSION},
                                            {"nms_build", ver},
                                            {"functions_found", ok},
                                            {"functions_total", static_cast<int>(FN_COUNT)}}}};
    g_http = std::make_unique<olympian::HttpServer>();
    std::string err;
    bool http_ok = g_http->Start(
        kHttpPort,
        [](const olympian::HttpRequest& req) {
            olympian::Json extra = g_debug_extra;
            extra["dll"]["frames"] = g_frames.load();
            extra["dll"]["frames_with_game"] = g_frames_with_game.load();
            return olympian::Route(*g_core, req, extra);
        },
        &err);
    if (http_ok)
        Log("Servidor para el overlay: http://127.0.0.1:%d/OlympianPlanetCoords/coords", kHttpPort);
    else
        Log("No se pudo abrir el servidor del overlay (%s). Si NMS.py esta abierto, cierralo: usan el mismo "
            "puerto.", err.c_str());

    // Engancha cGcApplication::Update (se ejecuta una vez por frame en el hilo del juego).
    int count = 0;
    void* target = reinterpret_cast<void*>(FindPattern(ParsePattern(kNmsSigs[FN_cGcApplication_Update].pattern), &count));
    MH_STATUS mh = MH_Initialize();
    if (mh == MH_OK)
        mh = MH_CreateHook(target, reinterpret_cast<void*>(&HookedUpdate), reinterpret_cast<void**>(&g_orig_update));
    if (mh == MH_OK) mh = MH_EnableHook(target);
    Log("Hook cGcApplication::Update: %s", MH_StatusToString(mh));
    Log(mh == MH_OK ? "LISTO: coordenadas en vivo activas." : "El hook fallo: el mod queda desactivado.");
    return 0;
}

}  // namespace

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = inst;
        DisableThreadLibraryCalls(inst);
        InitializeCriticalSection(&g_log_lock);
        if (!LoadRealWinmm()) return FALSE;  // sin el winmm real el juego no podria funcionar
        HANDLE h = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
        if (h) CloseHandle(h);
    }
    return TRUE;
}
