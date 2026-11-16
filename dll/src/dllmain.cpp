// Olympian Planet Coords (DLL) - fase 1: cargador + sonda de compatibilidad
//
// Desarrollado por MightyZeus, originario de Nicaragua - Copyright (c) 2026 MightyZeus
// GitHub: https://github.com/ImMightyZeus  -  Apoyo: https://paypal.me/ImMightyZeus
// Agradecimientos a mi querida esposa, a mi familia y a Pangro Daer Barun D'Gross por seguirme
// corrompiendo con videojuegos.
//
// Se instala como "winmm.dll" junto a NMS.exe. Windows lo carga en lugar del winmm.dll del
// sistema; este DLL carga el verdadero (System32) y le reenvia todas las llamadas, asi que el
// juego funciona igual. Ademas:
//   1. Comprueba que la version de NMS.exe sea la esperada (si no, no hace nada mas).
//   2. Busca en NMS.exe las funciones que usara el mod (patrones de NMS.py) y lo anota en
//      Binaries\Olympian\olympian.log.
// En esta fase NO instala ningun hook ni toca la memoria del juego.

#include <windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "MinHook.h"
#include "nms_sigs.h"
#include "winmm_exports.h"

#define OLYMPIAN_VERSION "0.0.1-dll-fase1"

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
    int ok = 0;
    for (int i = 0; i < FN_COUNT; ++i) {
        int count = 0;
        uintptr_t addr = FindPattern(ParsePattern(kNmsSigs[i].pattern), &count);
        Log("%-40s %s  rva=0x%llx  coincidencias=%d", kNmsSigs[i].name, addr ? "OK " : "NO ",
            static_cast<unsigned long long>(addr ? addr - base : 0), count);
        if (addr && count == 1) ++ok;
    }
    MH_STATUS mh = MH_Initialize();
    Log("MinHook: %s", MH_StatusToString(mh));
    if (mh == MH_OK) MH_Uninitialize();
    Log("RESULTADO: %d/%d funciones encontradas de forma unica. Fase 1 no instala hooks.", ok,
        static_cast<int>(FN_COUNT));
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
