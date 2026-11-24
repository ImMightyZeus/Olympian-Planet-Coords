// Olympian Planet Coords — teclado dentro del juego (ver input_hook.hpp).
// Olympian Planet Coords — in-game keyboard (see input_hook.hpp).
//
// Olympian Planet Coords — Copyright © 2026 MightyZeus · GPL-3.0 License
#include "input_hook.hpp"

#include <xinput.h>

#include <atomic>
#include <bitset>
#include <cstring>
#include <mutex>

#include "MinHook.h"

namespace olympian {

namespace {

InputLogFn g_log = nullptr;
std::atomic<bool> g_capture{false};
std::mutex g_mu;
std::vector<UiEvent> g_events;
KeyTracker g_keys;
wchar_t g_dead = 0;  // acento pendiente (tecla muerta, por ejemplo ´ en el teclado espanol)
// Teclas que se presionaron para el panel y todavia no se sueltan. Su "soltar" tampoco le llega al juego,
// aunque el panel ya se haya cerrado (asi Esc o Enter al cerrar no abren el menu del juego).
std::bitset<256> g_raw_held, g_wm_held;
std::atomic<bool> g_holding{false};
ULONGLONG g_released_at = 0;

bool Filtering() { return g_capture.load() || g_holding.load(); }

using GetRawInputDataFn = UINT(WINAPI*)(HRAWINPUT, UINT, LPVOID, PUINT, UINT);
using GetAsyncKeyStateFn = SHORT(WINAPI*)(int);
using GetKeyStateFn = SHORT(WINAPI*)(int);
using GetKeyboardStateFn = BOOL(WINAPI*)(PBYTE);
using XInputGetStateFn = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);

GetRawInputDataFn o_raw = nullptr;
GetAsyncKeyStateFn o_async = nullptr;
GetKeyStateFn o_keystate = nullptr;
GetKeyboardStateFn o_kbstate = nullptr;
XInputGetStateFn o_xinput = nullptr;

HWND g_hwnd = nullptr;
WNDPROC g_old_proc = nullptr;
bool g_proc_unicode = true;

UINT WINAPI HookRaw(HRAWINPUT h, UINT cmd, LPVOID data, PUINT size, UINT header) {
    UINT r = o_raw(h, cmd, data, size, header);
    if (!Filtering() || cmd != RID_INPUT || !data || r == static_cast<UINT>(-1) || r < sizeof(RAWINPUTHEADER))
        return r;
    RAWINPUT* ri = static_cast<RAWINPUT*>(data);
    if (ri->header.dwType == RIM_TYPEKEYBOARD) {
        RAWKEYBOARD& k = ri->data.keyboard;
        const USHORT vk = k.VKey;
        const bool up = (k.Flags & RI_KEY_BREAK) != 0;
        if (up) {
            if (g_capture.load()) {
                std::vector<UiEvent> ev;
                TranslateKey(k, &g_keys, GetKeyboardLayout(0), &ev);  // lleva Shift, AltGr... (antes de vaciarla)
            }
            if (vk < 256 && g_raw_held[vk]) {  // se presiono para el panel: el juego tampoco ve que se suelta
                std::lock_guard<std::mutex> g(g_mu);
                g_raw_held.reset(vk);
                g_holding.store(g_raw_held.any() || g_wm_held.any());
                k.VKey = 0xFF;
                k.MakeCode = 0;
                k.Message = WM_NULL;
            }
            return r;
        }
        if (!g_capture.load()) return r;
        std::vector<UiEvent> ev;
        TranslateKey(k, &g_keys, GetKeyboardLayout(0), &ev);
        std::lock_guard<std::mutex> g(g_mu);
        g_events.insert(g_events.end(), ev.begin(), ev.end());
        if (vk < 256) g_raw_held.set(vk);
        g_holding.store(true);
        NeutralizeRaw(ri);
        return r;
    }
    if (g_capture.load()) NeutralizeRaw(ri);
    return r;
}

// Mientras el panel captura (o quedan teclas suyas presionadas): "nada presionado".
SHORT WINAPI HookAsync(int vk) { return Filtering() ? 0 : o_async(vk); }
SHORT WINAPI HookKeyState(int vk) { return Filtering() ? 0 : o_keystate(vk); }
BOOL WINAPI HookKbState(PBYTE s) {
    BOOL r = o_kbstate(s);
    if (r && s && Filtering()) std::memset(s, 0, 256);
    return r;
}
DWORD WINAPI HookXInput(DWORD user, XINPUT_STATE* st) {
    DWORD r = o_xinput(user, st);
    if (r == ERROR_SUCCESS && st && g_capture.load()) std::memset(&st->Gamepad, 0, sizeof(st->Gamepad));
    return r;
}

// Mensajes de teclado y clics: mientras se captura, no llegan al juego.
LRESULT CALLBACK HookProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if ((msg == WM_KEYUP || msg == WM_SYSKEYUP) && wp < 256 && g_wm_held[wp]) {
        std::lock_guard<std::mutex> g(g_mu);
        g_wm_held.reset(wp);
        g_holding.store(g_raw_held.any() || g_wm_held.any());
        return 0;
    }
    if (g_capture.load() && (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) && wp < 256) {
        std::lock_guard<std::mutex> g(g_mu);
        g_wm_held.set(wp);
        g_holding.store(true);
        return 0;
    }
    if (g_capture.load()) {
        switch (msg) {
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
            case WM_CHAR:
            case WM_SYSCHAR:
            case WM_DEADCHAR:
            case WM_SYSDEADCHAR:
            case WM_UNICHAR:
            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_XBUTTONDOWN:
            case WM_LBUTTONDBLCLK:
            case WM_RBUTTONDBLCLK:
            case WM_MBUTTONDBLCLK:
            case WM_MOUSEWHEEL:
            case WM_MOUSEHWHEEL:
                return 0;
            default:
                break;
        }
    }
    return g_proc_unicode ? CallWindowProcW(g_old_proc, hwnd, msg, wp, lp)
                          : CallWindowProcA(g_old_proc, hwnd, msg, wp, lp);
}

void HookWindow() {
    HWND fg = GetForegroundWindow();
    DWORD pid = 0;
    if (!fg || (GetWindowThreadProcessId(fg, &pid), pid != GetCurrentProcessId()) || fg == g_hwnd) return;
    g_proc_unicode = IsWindowUnicode(fg) != FALSE;  // la ventana sigue siendo ANSI o Unicode, como era
    LONG_PTR mine = reinterpret_cast<LONG_PTR>(&HookProc);
    LONG_PTR old = g_proc_unicode ? SetWindowLongPtrW(fg, GWLP_WNDPROC, mine) : SetWindowLongPtrA(fg, GWLP_WNDPROC, mine);
    if (old) {
        g_hwnd = fg;
        g_old_proc = reinterpret_cast<WNDPROC>(old);
        if (g_log) g_log("[teclado] ventana del juego enganchada (%s)", g_proc_unicode ? "Unicode" : "ANSI");
    }
}

template <typename T>
bool Hook(const wchar_t* module, const char* name, void* detour, T* orig) {
    MH_STATUS s = MH_CreateHookApi(module, name, detour, reinterpret_cast<void**>(orig));
    if (s == MH_OK) s = MH_EnableHook(MH_ALL_HOOKS);
    if (g_log) g_log("[teclado] %s: %s", name, MH_StatusToString(s));
    return s == MH_OK;
}

// Tecla muerta + letra: la letra acentuada (por ejemplo ´ + e = é).
wchar_t Compose(wchar_t dead, wchar_t base) {
    wchar_t mark = 0;
    switch (dead) {
        case 0x00B4: case 0x0027: mark = 0x0301; break;  // ´
        case 0x0060: mark = 0x0300; break;                // `
        case 0x00A8: case 0x0022: mark = 0x0308; break;  // ¨
        case 0x005E: mark = 0x0302; break;                // ^
        case 0x007E: mark = 0x0303; break;                // ~
        default: return 0;
    }
    wchar_t in[3] = {base, mark, 0}, out[4] = {0};
    int n = NormalizeString(NormalizationC, in, 2, out, 4);
    return n == 1 ? out[0] : 0;
}

}  // namespace

void KeyTracker::Reset(bool caps, bool num) {
    std::memset(state, 0, sizeof(state));
    if (caps) state[VK_CAPITAL] = 0x01;
    if (num) state[VK_NUMLOCK] = 0x01;
}

bool TranslateKey(const RAWKEYBOARD& k, KeyTracker* t, HKL layout, std::vector<UiEvent>* out) {
    const bool up = (k.Flags & RI_KEY_BREAK) != 0;
    const bool e0 = (k.Flags & RI_KEY_E0) != 0;
    USHORT vk = k.VKey;
    if (vk == 0 || vk >= 0xFF) return false;
    // Modificadores: Shift izquierdo/derecho, Ctrl, Alt (AltGr = Alt derecho, que Windows ve como Ctrl+Alt).
    auto set = [t, up](int v) { t->state[v] = up ? 0 : 0x80; };
    switch (vk) {
        case VK_SHIFT:
            set(k.MakeCode == 0x36 ? VK_RSHIFT : VK_LSHIFT);
            t->state[VK_SHIFT] = (t->state[VK_LSHIFT] | t->state[VK_RSHIFT]) & 0x80;
            return !up;
        case VK_CONTROL:
            set(e0 ? VK_RCONTROL : VK_LCONTROL);
            t->state[VK_CONTROL] = (t->state[VK_LCONTROL] | t->state[VK_RCONTROL]) & 0x80;
            return !up;
        case VK_MENU:
            set(e0 ? VK_RMENU : VK_LMENU);
            t->state[VK_MENU] = (t->state[VK_LMENU] | t->state[VK_RMENU]) & 0x80;
            if (e0) {  // AltGr: Ctrl + Alt para ToUnicodeEx
                t->state[VK_CONTROL] = up ? (t->state[VK_LCONTROL] | t->state[VK_RCONTROL]) & 0x80 : 0x80;
            }
            return !up;
        case VK_CAPITAL:
            if (!up) t->state[VK_CAPITAL] ^= 0x01;
            return !up;
        case VK_NUMLOCK:
            if (!up) t->state[VK_NUMLOCK] ^= 0x01;
            return !up;
        default:
            break;
    }
    if (up) return false;
    auto push = [out](UiKey key, char32_t ch = 0) { out->push_back(UiEvent{key, ch}); };
    switch (vk) {
        case VK_BACK: push(UiKey::Back); return true;
        case VK_RETURN: push(UiKey::Enter); return true;
        case VK_ESCAPE: push(UiKey::Esc); return true;
        case VK_TAB: push(UiKey::Tab); return true;
        case VK_UP: push(UiKey::Up); return true;
        case VK_DOWN: push(UiKey::Down); return true;
        case VK_LEFT: push(UiKey::Left); return true;
        case VK_RIGHT: push(UiKey::Right); return true;
        case VK_DELETE: push(UiKey::Del); return true;
        default: break;
    }
    if (vk >= VK_F1 && vk <= VK_F24) return true;  // las F no escriben nada (F9 la atiende el DLL aparte)
    // Ctrl sin Alt (atajos): no escribe
    if ((t->state[VK_CONTROL] & 0x80) && !(t->state[VK_MENU] & 0x80)) return true;
    wchar_t buf[8] = {0};
    UINT scan = k.MakeCode | (e0 ? 0xE000 : 0);
    int n = ToUnicodeEx(vk, scan, t->state, buf, 8, 0x4 /* no cambia el estado del teclado */, layout);
    if (n < 0) {  // tecla muerta: se guarda el acento para la letra siguiente
        g_dead = buf[0];
        return true;
    }
    for (int i = 0; i < n; ++i) {
        wchar_t c = buf[i];
        if (c < 0x20 || c == 0x7F) continue;
        if (g_dead) {
            wchar_t composed = Compose(g_dead, c);
            if (composed) {
                c = composed;
            } else if (c != L' ') {
                push(UiKey::Char, g_dead);  // sin combinacion: el acento solo y luego la letra
            } else {
                c = g_dead;  // acento + espacio = el acento
            }
            g_dead = 0;
        }
        push(UiKey::Char, static_cast<char32_t>(c));
    }
    return true;
}

bool NeutralizeRaw(RAWINPUT* ri) {
    if (ri->header.dwType == RIM_TYPEKEYBOARD) {
        if (ri->data.keyboard.Flags & RI_KEY_BREAK) return false;  // soltar si le llega al juego
        ri->data.keyboard.VKey = 0xFF;
        ri->data.keyboard.MakeCode = 0;
        ri->data.keyboard.Message = WM_NULL;
        return true;
    }
    if (ri->header.dwType == RIM_TYPEMOUSE) {
        RAWMOUSE& m = ri->data.mouse;
        // Solo se dejan pasar los botones que se sueltan (nada queda presionado en el juego).
        const USHORT ups = RI_MOUSE_LEFT_BUTTON_UP | RI_MOUSE_RIGHT_BUTTON_UP | RI_MOUSE_MIDDLE_BUTTON_UP |
                           RI_MOUSE_BUTTON_4_UP | RI_MOUSE_BUTTON_5_UP;
        m.usButtonFlags &= ups;
        m.usButtonData = 0;
        if (!(m.usFlags & MOUSE_MOVE_ABSOLUTE)) {
            m.lLastX = 0;
            m.lLastY = 0;
        }
        return true;
    }
    return false;
}

int InputHookInstall(InputLogFn log) {
    g_log = log;
    int ok = 0;
    ok += Hook(L"user32", "GetRawInputData", reinterpret_cast<void*>(&HookRaw), &o_raw);
    ok += Hook(L"user32", "GetAsyncKeyState", reinterpret_cast<void*>(&HookAsync), &o_async);
    ok += Hook(L"user32", "GetKeyState", reinterpret_cast<void*>(&HookKeyState), &o_keystate);
    ok += Hook(L"user32", "GetKeyboardState", reinterpret_cast<void*>(&HookKbState), &o_kbstate);
    if (GetModuleHandleW(L"xinput9_1_0"))
        ok += Hook(L"xinput9_1_0", "XInputGetState", reinterpret_cast<void*>(&HookXInput), &o_xinput);
    else if (g_log)
        g_log("[teclado] xinput9_1_0 no esta cargado: el mando no se bloquea");
    return ok;
}

void InputSetCapture(bool on) {
    if (on == g_capture.load()) return;
    if (on) {
        HookWindow();
        {
            std::lock_guard<std::mutex> g(g_mu);
            g_events.clear();
        }
        SHORT caps = o_keystate ? o_keystate(VK_CAPITAL) : GetKeyState(VK_CAPITAL);
        SHORT num = o_keystate ? o_keystate(VK_NUMLOCK) : GetKeyState(VK_NUMLOCK);
        g_keys.Reset(caps & 1, num & 1);
        g_dead = 0;
    }
    if (!on) g_released_at = GetTickCount64();
    g_capture.store(on);
    if (g_log) g_log("[teclado] captura %s", on ? "activada (panel abierto)" : "desactivada");
}

bool InputCapturing() { return g_capture.load(); }

void InputTick() {
    // Si una tecla del panel nunca se solto a la vista (por ejemplo, Alt+Tab con la tecla presionada),
    // a los 2 s de cerrar el panel se deja de filtrar igual.
    if (!g_capture.load() && g_holding.load() && GetTickCount64() - g_released_at > 2000) {
        std::lock_guard<std::mutex> g(g_mu);
        g_raw_held.reset();
        g_wm_held.reset();
        g_holding.store(false);
    }
}

void InputDrain(std::vector<UiEvent>* out) {
    std::lock_guard<std::mutex> g(g_mu);
    out->insert(out->end(), g_events.begin(), g_events.end());
    g_events.clear();
}

SHORT InputRealAsyncKeyState(int vk) { return o_async ? o_async(vk) : GetAsyncKeyState(vk); }

}  // namespace olympian
