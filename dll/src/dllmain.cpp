// Olympian Planet Coords (DLL) - paso 3: coordenadas, destinos, marcador en el HUD y tecla
// Olympian Planet Coords (DLL) - step 3: coordinates, destinations, HUD marker and hotkey
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
//   2. Busca en NMS.exe las funciones del juego (patrones propios, tools/sigs_<build>.json) y lo anota.
//   3. Engancha cGcApplication::Update: en cada frame LEE (nunca escribe) la posicion del jugador
//      y el planeta, y calcula las coordenadas igual que la version Python v0.0.3.
//   4. Sirve esas coordenadas en http://127.0.0.1:5000/OlympianPlanetCoords/ para el overlay, y
//      recibe de ahi los destinos (PUT /marker, /clear), la tecla (PUT /hotkey) y el idioma.
//   5. Engancha cGcBaseBuildingManager::Update: si hay destino en el planeta, despues de que el
//      juego actualiza sus marcadores, le pide al juego un marcador con
//      cGcBaseBuildingManager::AddHUDMarker (igual que el metodo BaseBuildingHUD de la version
//      Python, probado en el juego). El marcador dura un frame y se vuelve a pedir en el siguiente:
//      no queda nada guardado en la partida.
//   6. Tecla (F9 por defecto): con el juego en primer plano, abre el cuadro para escribir
//      coordenadas en el overlay.
//   7. v0.2 (en desarrollo): dibuja un recuadro con las coordenadas dentro del juego, copiandolo en
//      cada imagen antes de que el juego la muestre (vk_overlay.cpp). Se apaga con "ingame_box": false
//      en los ajustes de datos.json.

#include <windows.h>

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <atomic>
#include <cmath>
#include <memory>

#include "MinHook.h"
#include "core.hpp"
#include "game_reader.hpp"
#include "http_server.hpp"
#include "ingame_ui.hpp"
#include "input_hook.hpp"
#include "nms_offsets.h"
#include "nms_sigs.h"
#include "routes.hpp"
#include "vk_overlay.hpp"
#include "vk_probe.hpp"
#include "winmm_exports.h"

#define OLYMPIAN_VERSION "0.2.0-beta"
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

void Log(const char* fmt, ...);

// ---- estado del mod --------------------------------------------------------------------------
using UpdateFn = void (*)(void*);
UpdateFn g_orig_update = nullptr;
using StateChangeFn = void (*)(void*, void*, void*, uintptr_t);
StateChangeFn g_orig_state_change = nullptr;
// Objetos cTkFSM vistos en cTkFSM::StateChange. Uno de ellos es cGcApplication (como en NMS.py).
constexpr int kMaxCandidates = 16;
std::atomic<uintptr_t> g_candidates[kMaxCandidates];
std::atomic<int> g_candidate_count{0};
std::atomic<uintptr_t> g_app_valid{0};
uintptr_t g_global_g = 0;  // puntero global que lee cGcApplication::Update (solo para diagnostico)
std::unique_ptr<olympian::Core> g_core;
// Panel dentro del juego (escribir destinos, lista, ajustes). Lo abre la tecla del panel (F9).
std::unique_ptr<olympian::Panel> g_panel;
std::atomic<olympian::Panel*> g_panel_box{nullptr};
bool g_panel_key_was_down = false;
int g_input_hooks = 0;
std::unique_ptr<olympian::HttpServer> g_http;
olympian::Json g_debug_extra = olympian::Json::object();
std::atomic<unsigned long long> g_frames{0};
std::atomic<unsigned long long> g_frames_with_game{0};
std::atomic<uintptr_t> g_app{0};
unsigned long long g_start_s = 0;

// cGcBaseBuildingManager::Update(this, float lfTimeStep) y AddHUDMarker (desensamblado en 185034:
// rcx=this, dl=tipo, r8=matriz cTkPhysRelMat34 (lee +0x30 local y +0x40 offset), r9d=color,
// [rsp+0x28]=nombre cTkFixedString0x40). Lee this+0x60 -> +0x1e8 (tabla de colores).
using BbmUpdateFn = void (*)(void*, float);
BbmUpdateFn g_orig_bbm_update = nullptr;
using AddHudMarkerFn = void (*)(void*, int, const void*, int, const char*);
AddHudMarkerFn g_add_hud_marker = nullptr;

// Nombres que da el juego (opcionales, ver kNmsOptSigs). Se llaman solo desde Tick (hilo del juego).
using SystemNameFn = void (*)(void* solar_system, char* out);
using StationNameFn = void (*)(void* unused, uint64_t ua, char* out);
SystemNameFn g_system_name = nullptr;
StationNameFn g_station_name = nullptr;
constexpr size_t kNameBuf = 0x200;  // el juego escribe hasta 0x80

std::string NameFromBuf(const char* buf) {
    size_t n = strnlen(buf, kNameBuf - 1);
    std::string r(buf, n);
    while (!r.empty() && (r.back() == ' ' || r.back() == '\t')) r.pop_back();
    return r;
}
std::atomic<unsigned long long> g_bbm_frames{0};

struct alignas(16) Vec3f {
    float x, y, z, w;
};
struct alignas(16) PhysRelMat34 {  // cTkPhysRelMat34 (0x50 bytes)
    Vec3f right, up, at;
    Vec3f pos_local, pos_offset;  // cTkPhysRelVec3
};
static_assert(sizeof(PhysRelMat34) == 0x50, "cTkPhysRelMat34");
static_assert(offsetof(PhysRelMat34, pos_local) == 0x30 && offsetof(PhysRelMat34, pos_offset) == 0x40,
              "cTkPhysRelMat34.pos");

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

// Despues de que el juego actualiza la construccion de bases (y sus marcadores del HUD), se pide el
// marcador del destino. Solo si el juego tiene listo lo que AddHUDMarker usa por dentro.
void HookedBbmUpdate(void* self, float dt) {
    g_orig_bbm_update(self, dt);
    ++g_bbm_frames;
    if (!g_core || !self || !g_add_hud_marker) return;
    try {
        olympian::MarkerSpec m = g_core->Marker();
        if (!m.valid) return;
        uintptr_t inner = olympian::ReadPointer(reinterpret_cast<uintptr_t>(self) + 0x60);
        if (!inner || !olympian::ReadPointer(inner + 0x1e8)) {
            g_core->NoteMarkerCall(false);
            return;
        }
        PhysRelMat34 mat{};
        mat.right = {1, 0, 0, 0};
        mat.up = {0, 1, 0, 0};
        mat.at = {0, 0, 1, 0};
        mat.pos_local = {m.local[0], m.local[1], m.local[2], 0};
        mat.pos_offset = {m.offset[0], m.offset[1], m.offset[2], 0};
        alignas(16) char label[64];
        std::memcpy(label, m.label, sizeof(label));
        g_add_hud_marker(self, olympian::kMarkerTypeCustom, &mat, 0, label);
        g_core->NoteMarkerCall(true);
    } catch (...) {
    }
}

// true si la ventana en primer plano es del juego (la tecla solo cuenta asi, como en NMS.py).
bool GameHasFocus() {
    HWND fg = GetForegroundWindow();
    if (!fg) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId();
}

struct FindWin {
    HWND best = nullptr;
    long long area = 0;
};
BOOL CALLBACK FindGameWindow(HWND h, LPARAM lp) {
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid != GetCurrentProcessId() || !IsWindowVisible(h) || GetWindow(h, GW_OWNER)) return TRUE;
    RECT r;
    if (!GetWindowRect(h, &r)) return TRUE;
    auto* f = reinterpret_cast<FindWin*>(lp);
    long long a = static_cast<long long>(r.right - r.left) * (r.bottom - r.top);
    if (a > f->area) {
        f->area = a;
        f->best = h;
    }
    return TRUE;
}

// Devuelve el foco al juego despues de escribir el destino en el overlay (como
// set_main_window_active de NMS.py).
void FocusGame() {
    if (GameHasFocus()) return;
    FindWin f;
    EnumWindows(FindGameWindow, reinterpret_cast<LPARAM>(&f));
    if (!f.best) return;
    if (SetForegroundWindow(f.best)) return;
    // Windows a veces no deja cambiar el foco desde otro proceso: se une un momento a la entrada
    // de la ventana que tiene el foco (el overlay) y se intenta de nuevo.
    HWND fg = GetForegroundWindow();
    DWORD fg_thread = fg ? GetWindowThreadProcessId(fg, nullptr) : 0, me = GetCurrentThreadId();
    if (fg_thread && fg_thread != me && AttachThreadInput(me, fg_thread, TRUE)) {
        SetForegroundWindow(f.best);
        AttachThreadInput(me, fg_thread, FALSE);
    }
}

std::string SystemLang() {
    switch (GetUserDefaultUILanguage() & 0x3FF) {  // idioma principal de Windows
        case 0x0A: return "es";
        case 0x19: return "ru";
        default: return "en";
    }
}

// cTkFSM::StateChange: anota el objeto (cGcApplication es un cTkFSM). No cambia nada del juego.
void HookedStateChange(void* self, void* a, void* b, uintptr_t c) {
    g_orig_state_change(self, a, b, c);
    uintptr_t p = reinterpret_cast<uintptr_t>(self);
    int n = g_candidate_count.load();
    for (int i = 0; i < n; ++i)
        if (g_candidates[i].load() == p) return;
    if (n < kMaxCandidates) {
        g_candidates[n].store(p);
        g_candidate_count.store(n + 1);
    }
}

// Se ejecuta en el hilo del juego, una vez por frame. Primero el juego hace lo suyo; despues se lee.
// cGcApplication::Update NO recibe el puntero de la aplicacion en rcx (usa un global), por eso el
// objeto se toma de cTkFSM::StateChange y se valida con lo que se lee.
void HookedUpdate(void* arg) {
    g_orig_update(arg);
    ++g_frames;
    if (!g_core) return;
    try {
        olympian::GameView view;
        bool ok = false;
        uintptr_t app = g_app_valid.load();
        if (app) {
            ok = olympian::ReadGameView(app, &view) && view.have_player_env;
        } else {
            int n = g_candidate_count.load();
            for (int i = 0; i < n && !ok; ++i) {
                uintptr_t cand = g_candidates[i].load();
                olympian::GameView v;
                if (olympian::ReadGameView(cand, &v) && olympian::PlausibleView(v)) {
                    g_app_valid.store(cand);
                    g_app.store(cand);
                    view = v;
                    ok = true;
                    Log("cGcApplication reconocido (candidato %d de %d).", i + 1, n);
                }
            }
        }
        if (ok) ++g_frames_with_game;
        g_core->Tick(ok ? &view : nullptr, static_cast<double>(GetTickCount64()) / 1000.0);
        int vk = g_core->HotkeyVk();
        // GetAsyncKeyState verdadero: el del juego devuelve "nada" mientras el panel captura el teclado.
        bool down = vk && (olympian::InputRealAsyncKeyState(vk) & 0x8000) != 0;
        bool focused = GameHasFocus();
        if (g_panel && g_input_hooks > 0 && g_core->InGameBox()) {
            // Todo dentro del juego: la tecla abre y cierra el panel (el recuadro .exe ya no se abre).
            bool press = down && !g_panel_key_was_down;
            g_panel_key_was_down = down;
            if (press && focused) g_panel->Toggle();
            if (!focused && g_panel->IsOpen()) g_panel->Close();  // Alt+Tab: el panel se cierra
            olympian::InputSetCapture(g_panel->IsOpen() && focused);
            olympian::InputTick();
            std::vector<olympian::UiEvent> ev;
            olympian::InputDrain(&ev);
            for (const auto& e : ev) g_panel->OnKey(e);
            if (!g_panel->IsOpen()) olympian::InputSetCapture(false);
        } else {
            olympian::InputSetCapture(false);
            g_core->KeyState(down, down && focused);  // recuadro .exe (si el recuadro del juego esta apagado)
        }
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

HWND GameWindow() {
    FindWin f;
    EnumWindows(FindGameWindow, reinterpret_cast<LPARAM>(&f));
    return f.best;
}

std::atomic<olympian::Core*> g_core_box{nullptr};

std::wstring Wide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &w[0], n);
    return w;
}

// Lineas del recuadro dentro del juego (titulo, coordenadas o motivo, destino). Vacio = no dibujar.
std::wstring BoxText() {
    olympian::Core* c = g_core_box.load();
    if (!c || !c->InGameBox()) return L"";
    if (olympian::Panel* p = g_panel_box.load()) {
        std::string t = p->Text();
        if (!t.empty()) return Wide(t);
    }
    olympian::Json s = olympian::Json::parse(c->CoordsJson(), nullptr, false);
    if (s.is_discarded()) return L"";
    // Flecha suave, como en el .exe: se acerca al angulo nuevo por el camino corto (12 por segundo).
    static bool have = false;
    static double sy = 0, sp = 0;
    static ULONGLONG last = 0;
    ULONGLONG now = GetTickCount64();
    if (s.contains("destination") && s["destination"].is_object() && s["destination"].contains("yaw") &&
        s["destination"]["yaw"].is_number()) {
        double ty = s["destination"]["yaw"].get<double>(), tp = s["destination"].value("pitch", 0.0);
        if (!have) {
            sy = ty;
            sp = tp;
            have = true;
        } else {
            double dt = std::min(0.2, (now - last) / 1000.0), f = 1.0 - std::exp(-dt * 12.0);
            double diff = std::fmod(ty - sy + 540.0, 360.0) - 180.0;
            sy += diff * f;
            if (sy > 180) sy -= 360;
            if (sy < -180) sy += 360;
            sp += (tp - sp) * f;
        }
        s["destination"]["yaw"] = sy;
        s["destination"]["pitch"] = sp;
    } else {
        have = false;
    }
    last = now;
    return Wide(olympian::BoxLines(s, c->Lang()));
}

// Informes de la sonda de Vulkan: a los 20 s, 1 min, 3 min y luego cada 10 min.
DWORD WINAPI VkReportThread(LPVOID) {
    const DWORD waits[] = {20000, 40000, 120000};
    char when[32];
    for (int i = 0;; ++i) {
        Sleep(i < 3 ? waits[i] : 600000);
        snprintf(when, sizeof(when), "%llu s", static_cast<unsigned long long>(GetTickCount64() / 1000 - g_start_s));
        olympian::VkProbeReport(when, GameWindow());
    }
}

DWORD WINAPI Worker(LPVOID) {
    g_start_s = GetTickCount64() / 1000;
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
    // Sonda de Vulkan y recuadro dentro del juego: lo antes posible, para ver la creacion del dispositivo.
    olympian::VkProbeInstall(&Log, &BoxText);
    CreateThread(nullptr, 0, VkReportThread, nullptr, 0, nullptr);
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
    // Nombres de sistema y estacion (opcional: si no se encuentran, se muestran sistema/galaxia de reserva).
    {
        int c1 = 0, c2 = 0;
        uintptr_t a1 = FindPattern(ParsePattern(kNmsOptSigs[OPT_cGcSolarSystem_GetDisplayName].pattern), &c1);
        uintptr_t a2 = FindPattern(ParsePattern(kNmsOptSigs[OPT_cGcNameGenerator_GenerateSpaceStationName].pattern), &c2);
        if (a1 && c1 == 1) g_system_name = reinterpret_cast<SystemNameFn>(a1);
        if (a2 && c2 == 1) g_station_name = reinterpret_cast<StationNameFn>(a2);
        Log("Nombres del juego: sistema %s (rva=0x%llx), estacion %s (rva=0x%llx)", g_system_name ? "OK" : "NO",
            static_cast<unsigned long long>(g_system_name ? a1 - base : 0), g_station_name ? "OK" : "NO",
            static_cast<unsigned long long>(g_station_name ? a2 - base : 0));
        olympian::NameSource ns;
        if (g_system_name)
            ns.system = [](uint64_t) -> std::string {
                uintptr_t app = g_app_valid.load();
                uintptr_t data = app ? olympian::ReadPointer(app + nmsoff::APP_MPDATA) : 0;
                uintptr_t solar = data ? olympian::ReadPointer(data + nmsoff::DATA_SIMULATION + nmsoff::SIM_SOLAR_SYSTEM) : 0;
                // cGcSolarSystem completo legible (0x523E70 bytes no; basta con sus datos y nombre: 0x2700)
                if (!solar || !olympian::Readable(solar, 0x2700)) return "";
                char buf[kNameBuf] = {0};
                g_system_name(reinterpret_cast<void*>(solar), buf);
                return NameFromBuf(buf);
            };
        if (g_station_name)
            ns.station = [](uint64_t ua) -> std::string {
                char buf[kNameBuf] = {0};
                g_station_name(nullptr, ua, buf);
                return NameFromBuf(buf);
            };
        g_core->SetNameSource(std::move(ns));
    }
    g_core_box = g_core.get();
    g_panel = std::make_unique<olympian::Panel>(olympian::PanelHost{
        [] { return g_core->CoordsJson(); },
        [](const std::string& route, const olympian::Json& q) { return g_core->Put(route, q); }});
    g_panel_box = g_panel.get();
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
            extra["dll"]["fsm_candidates"] = g_candidate_count.load();
            uintptr_t app = g_app_valid.load();
            uintptr_t gval = g_global_g ? olympian::ReadPointer(g_global_g) : 0;
            char buf[64];
            snprintf(buf, sizeof(buf), "0x%llx", static_cast<unsigned long long>(app));
            extra["dll"]["app"] = buf;
            snprintf(buf, sizeof(buf), "0x%llx",
                     static_cast<unsigned long long>(app ? olympian::ReadPointer(app + 0x38) : 0));
            extra["dll"]["app_mpData"] = buf;
            snprintf(buf, sizeof(buf), "0x%llx", static_cast<unsigned long long>(gval));
            extra["dll"]["update_global"] = buf;
            extra["dll"]["bbm_frames"] = g_bbm_frames.load();
            extra["vulkan"] = olympian::Json::parse(olympian::VkProbeJson());
            extra["overlay"] = olympian::Json::parse(olympian::OverlayJson());
            extra["dll"]["input_hooks"] = g_input_hooks;
            extra["dll"]["panel_open"] = g_panel_box.load() && g_panel_box.load()->IsOpen();
            extra["dll"]["capturing"] = olympian::InputCapturing();
            olympian::HttpResponse res = olympian::Route(*g_core, req, extra);
            if (req.method == "PUT" && req.path.find("/marker") != std::string::npos &&
                res.body.find("\"queued\":true") != std::string::npos)
                FocusGame();
            return res;
        },
        &err);
    if (http_ok)
        Log("Servidor para el overlay: http://127.0.0.1:%d/OlympianPlanetCoords/coords", kHttpPort);
    else
        Log("No se pudo abrir el servidor del overlay (%s). Si NMS.py esta abierto, cierralo: usan el mismo "
            "puerto.", err.c_str());

    // Engancha cTkFSM::StateChange (para conocer cGcApplication) y cGcApplication::Update (una vez
    // por frame en el hilo del juego).
    int count = 0;
    void* target = reinterpret_cast<void*>(FindPattern(ParsePattern(kNmsSigs[FN_cGcApplication_Update].pattern), &count));
    void* sc_target =
        reinterpret_cast<void*>(FindPattern(ParsePattern(kNmsSigs[FN_cTkFSM_StateChange].pattern), &count));
    // Diagnostico: "mov rax, [rip+disp32]" en Update+0x77 (build 185034) lee un puntero global.
    {
        uint8_t* p = reinterpret_cast<uint8_t*>(target) + 0x77;
        if (p[0] == 0x48 && p[1] == 0x8B && p[2] == 0x05) {
            int32_t disp;
            memcpy(&disp, p + 3, 4);
            g_global_g = reinterpret_cast<uintptr_t>(p + 7) + disp;
        }
    }
    MH_STATUS mh = MH_Initialize();
    if (mh == MH_ERROR_ALREADY_INITIALIZED) mh = MH_OK;  // la sonda de Vulkan ya lo inicio
    // Teclado del panel dentro del juego (si falla, el panel no se abre y F9 sigue con el recuadro .exe).
    if (mh == MH_OK) {
        g_input_hooks = olympian::InputHookInstall(&Log);
        Log("Teclado del panel: %d de 6 enganches", g_input_hooks);
    }
    // Marcador del HUD: si este hook fallara, las coordenadas siguen funcionando igual.
    void* bbm_target = reinterpret_cast<void*>(
        FindPattern(ParsePattern(kNmsSigs[FN_cGcBaseBuildingManager_Update].pattern), &count));
    g_add_hud_marker = reinterpret_cast<AddHudMarkerFn>(
        FindPattern(ParsePattern(kNmsSigs[FN_cGcBaseBuildingManager_AddHUDMarker].pattern), &count));
    if (mh == MH_OK) {
        MH_STATUS mb = MH_CreateHook(bbm_target, reinterpret_cast<void*>(&HookedBbmUpdate),
                                     reinterpret_cast<void**>(&g_orig_bbm_update));
        if (mb == MH_OK) mb = MH_EnableHook(bbm_target);
        Log("Hook cGcBaseBuildingManager::Update (marcador del HUD): %s", MH_StatusToString(mb));
    }
    if (mh == MH_OK)
        mh = MH_CreateHook(sc_target, reinterpret_cast<void*>(&HookedStateChange),
                           reinterpret_cast<void**>(&g_orig_state_change));
    if (mh == MH_OK)
        mh = MH_CreateHook(target, reinterpret_cast<void*>(&HookedUpdate), reinterpret_cast<void**>(&g_orig_update));
    if (mh == MH_OK) mh = MH_EnableHook(MH_ALL_HOOKS);
    Log("Hooks cTkFSM::StateChange + cGcApplication::Update: %s", MH_StatusToString(mh));
    Log(mh == MH_OK ? "LISTO: coordenadas, destinos, marcador y tecla activos."
                    : "El hook fallo: el mod queda desactivado.");
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
