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

using GetCursorPosFn = BOOL(WINAPI*)(LPPOINT);
GetCursorPosFn o_cursor = nullptr;
XInputGetStateFn o_xinput14 = nullptr, o_xinput14ex = nullptr, o_xinput13 = nullptr;
POINT g_frozen_cursor{};
std::atomic<bool> g_raw_kbd{false};  // en esta captura llegan teclas por Raw Input (no se usan WM_KEYDOWN)

// Contadores de la captura actual (se anotan en el registro al cerrar el panel).
struct Counters {
    std::atomic<unsigned> wm_input_kbd{0}, wm_input_mouse{0}, wm_input_other{0}, keydown{0}, keyup{0}, chars{0},
        mouse_msgs{0}, raw_calls{0}, async_calls{0}, keystate_calls{0}, kbstate_calls{0}, cursor_calls{0},
        xinput_calls{0}, events{0};
    void Reset() {
        for (auto* c : {&wm_input_kbd, &wm_input_mouse, &wm_input_other, &keydown, &keyup, &chars, &mouse_msgs,
                        &raw_calls, &async_calls, &keystate_calls, &kbstate_calls, &cursor_calls, &xinput_calls,
                        &events})
            c->store(0);
    }
} g_n;

DWORD g_hook_thread = 0;
HHOOK g_msg_hook = nullptr;

void Push(const std::vector<UiEvent>& ev) {
    if (ev.empty()) return;
    std::lock_guard<std::mutex> g(g_mu);
    g_events.insert(g_events.end(), ev.begin(), ev.end());
    g_n.events += static_cast<unsigned>(ev.size());
}

// Una tecla (de Raw Input o de WM_KEYDOWN/WM_KEYUP) mientras se captura o quedan teclas del panel
// presionadas. Devuelve true si el juego NO debe verla.
bool KeyForPanel(const RAWKEYBOARD& k, bool translate, std::bitset<256>* held) {
    const USHORT vk = k.VKey;
    const bool up = (k.Flags & RI_KEY_BREAK) != 0;
    if (up) {
        if (g_capture.load() && translate) {
            std::vector<UiEvent> ev;
            TranslateKey(k, &g_keys, GetKeyboardLayout(0), &ev);  // lleva Shift, AltGr...
        }
        if (vk < 256 && (*held)[vk]) {  // se presiono para el panel: el juego tampoco ve que se suelta
            std::lock_guard<std::mutex> g(g_mu);
            held->reset(vk);
            g_holding.store(g_raw_held.any() || g_wm_held.any());
            return true;
        }
        return false;
    }
    if (!g_capture.load()) return false;
    if (translate) {
        std::vector<UiEvent> ev;
        TranslateKey(k, &g_keys, GetKeyboardLayout(0), &ev);
        Push(ev);
    }
    std::lock_guard<std::mutex> g(g_mu);
    if (vk < 256) held->set(vk);
    g_holding.store(true);
    return true;
}

UINT WINAPI HookRaw(HRAWINPUT h, UINT cmd, LPVOID data, PUINT size, UINT header) {
    UINT r = o_raw(h, cmd, data, size, header);
    if (!g_capture.load()) return r;
    ++g_n.raw_calls;
    // Respaldo: si el juego lee Raw Input por otro camino, le llega vacio (las teclas para el panel se
    // toman en FilterMessage, al llegar el mensaje WM_INPUT).
    if (cmd == RID_INPUT && data && r != static_cast<UINT>(-1) && r >= sizeof(RAWINPUTHEADER))
        NeutralizeRaw(static_cast<RAWINPUT*>(data));
    return r;
}

// Filtro de los mensajes que el juego saca de su cola (WH_GETMESSAGE en el hilo de la ventana).
void FilterMessage(MSG* m, bool removing) {
    const UINT msg = m->message;
    const bool cap = g_capture.load();
    auto drop = [m] { m->message = WM_NULL; };
    if (msg == WM_INPUT) {
        if (!removing) {
            if (cap) drop();
            return;
        }
        alignas(8) unsigned char buf[sizeof(RAWINPUT) + 64];
        UINT sz = sizeof(buf);
        UINT got = o_raw(reinterpret_cast<HRAWINPUT>(m->lParam), RID_INPUT, buf, &sz, sizeof(RAWINPUTHEADER));
        bool block = cap;
        if (got != static_cast<UINT>(-1) && got >= sizeof(RAWINPUTHEADER)) {
            RAWINPUT* ri = reinterpret_cast<RAWINPUT*>(buf);
            if (ri->header.dwType == RIM_TYPEKEYBOARD) {
                if (cap) {
                    ++g_n.wm_input_kbd;
                    g_raw_kbd.store(true);
                }
                block = KeyForPanel(ri->data.keyboard, true, &g_raw_held) || cap;
            } else if (ri->header.dwType == RIM_TYPEMOUSE) {
                if (cap) ++g_n.wm_input_mouse;
            } else if (cap) {
                ++g_n.wm_input_other;
            }
        }
        if (block) {
            DefWindowProcW(m->hwnd, WM_INPUT, m->wParam, m->lParam);  // libera el dato de Raw Input
            drop();
        }
        return;
    }
    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYUP) {
        const bool up = msg == WM_KEYUP || msg == WM_SYSKEYUP;
        if (!removing) {
            if (cap || (up && m->wParam < 256 && g_wm_held[m->wParam])) drop();
            return;
        }
        if (cap) ++(up ? g_n.keyup : g_n.keydown);
        RAWKEYBOARD k{};
        k.VKey = static_cast<USHORT>(m->wParam);
        k.MakeCode = static_cast<USHORT>((m->lParam >> 16) & 0xFF);
        k.Flags = static_cast<USHORT>((up ? RI_KEY_BREAK : RI_KEY_MAKE) | (((m->lParam >> 24) & 1) ? RI_KEY_E0 : 0));
        // Si las teclas llegan por Raw Input, esas se usan para el panel (aqui solo se bloquea).
        if (KeyForPanel(k, !g_raw_kbd.load(), &g_wm_held)) drop();
        return;
    }
    if (!cap) return;
    switch (msg) {
        case WM_CHAR:
        case WM_SYSCHAR:
        case WM_DEADCHAR:
        case WM_SYSDEADCHAR:
        case WM_UNICHAR:
            if (removing) ++g_n.chars;
            drop();
            return;
        default:
            break;
    }
    if (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) {
        if (removing) ++g_n.mouse_msgs;
        // Los botones que se sueltan si le llegan (nada queda presionado en el juego).
        if (msg != WM_LBUTTONUP && msg != WM_RBUTTONUP && msg != WM_MBUTTONUP && msg != WM_XBUTTONUP) drop();
    }
}

LRESULT CALLBACK GetMsgProc(int code, WPARAM wp, LPARAM lp) {
    if (code == HC_ACTION && lp && Filtering()) FilterMessage(reinterpret_cast<MSG*>(lp), wp == PM_REMOVE);
    return CallNextHookEx(nullptr, code, wp, lp);
}

// Engancha la cola de mensajes del hilo de la ventana del juego (la que tiene el foco).
void HookWindowThread() {
    HWND fg = GetForegroundWindow();
    DWORD pid = 0;
    DWORD tid = fg ? GetWindowThreadProcessId(fg, &pid) : 0;
    if (!tid || pid != GetCurrentProcessId() || tid == g_hook_thread) return;
    HHOOK h = SetWindowsHookExW(WH_GETMESSAGE, &GetMsgProc, nullptr, tid);
    if (!h) {
        if (g_log) g_log("[teclado] no se pudo enganchar la cola de mensajes (error %lu)", GetLastError());
        return;
    }
    if (g_msg_hook) UnhookWindowsHookEx(g_msg_hook);
    g_msg_hook = h;
    g_hook_thread = tid;
    if (g_log)
        g_log("[teclado] cola de mensajes enganchada (hilo %lu%s)", tid,
              tid == GetCurrentThreadId() ? ", el mismo del juego" : ", otro hilo");
}

BOOL WINAPI HookCursor(LPPOINT p) {
    if (g_capture.load() && p) {  // el cursor "no se mueve" mientras el panel esta abierto
        ++g_n.cursor_calls;
        *p = g_frozen_cursor;
        return TRUE;
    }
    return o_cursor(p);
}

DWORD WINAPI HookXInput14(DWORD user, XINPUT_STATE* st) {
    DWORD r = o_xinput14(user, st);
    if (r == ERROR_SUCCESS && st && g_capture.load()) std::memset(&st->Gamepad, 0, sizeof(st->Gamepad));
    return r;
}
DWORD WINAPI HookXInput14Ex(DWORD user, XINPUT_STATE* st) {
    DWORD r = o_xinput14ex(user, st);
    if (r == ERROR_SUCCESS && st && g_capture.load()) std::memset(&st->Gamepad, 0, sizeof(st->Gamepad));
    return r;
}
DWORD WINAPI HookXInput13(DWORD user, XINPUT_STATE* st) {
    DWORD r = o_xinput13(user, st);
    if (r == ERROR_SUCCESS && st && g_capture.load()) std::memset(&st->Gamepad, 0, sizeof(st->Gamepad));
    return r;
}

// Mientras el panel captura (o quedan teclas suyas presionadas): "nada presionado".
SHORT WINAPI HookAsync(int vk) {
    if (!Filtering()) return o_async(vk);
    ++g_n.async_calls;
    return 0;
}
SHORT WINAPI HookKeyState(int vk) {
    if (!Filtering()) return o_keystate(vk);
    ++g_n.keystate_calls;
    return 0;
}
BOOL WINAPI HookKbState(PBYTE s) {
    BOOL r = o_kbstate(s);
    if (r && s && Filtering()) {
        ++g_n.kbstate_calls;
        std::memset(s, 0, 256);
    }
    return r;
}
DWORD WINAPI HookXInput(DWORD user, XINPUT_STATE* st) {
    DWORD r = o_xinput(user, st);
    if (r == ERROR_SUCCESS && st && g_capture.load()) {
        ++g_n.xinput_calls;
        std::memset(&st->Gamepad, 0, sizeof(st->Gamepad));
    }
    return r;
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

// El mando: SDL carga xinput1_4 (o 1_3) por su cuenta y usa tambien XInputGetStateEx (ordinal 100).
void HookXInputLate() {
    static bool done14 = false, done13 = false;
    if (!done14 && GetModuleHandleW(L"xinput1_4")) {
        done14 = true;
        Hook(L"xinput1_4", "XInputGetState", reinterpret_cast<void*>(&HookXInput14), &o_xinput14);
        HMODULE m = GetModuleHandleW(L"xinput1_4");
        if (void* ex = reinterpret_cast<void*>(GetProcAddress(m, reinterpret_cast<LPCSTR>(100)))) {
            MH_STATUS st = MH_CreateHook(ex, reinterpret_cast<void*>(&HookXInput14Ex),
                                         reinterpret_cast<void**>(&o_xinput14ex));
            if (st == MH_OK) st = MH_EnableHook(ex);
            if (g_log) g_log("[teclado] XInputGetStateEx (1_4): %s", MH_StatusToString(st));
        }
    }
    if (!done13 && GetModuleHandleW(L"xinput1_3")) {
        done13 = true;
        Hook(L"xinput1_3", "XInputGetState", reinterpret_cast<void*>(&HookXInput13), &o_xinput13);
    }
}

int InputHookInstall(InputLogFn log) {
    g_log = log;
    int ok = 0;
    ok += Hook(L"user32", "GetRawInputData", reinterpret_cast<void*>(&HookRaw), &o_raw);
    ok += Hook(L"user32", "GetAsyncKeyState", reinterpret_cast<void*>(&HookAsync), &o_async);
    ok += Hook(L"user32", "GetKeyState", reinterpret_cast<void*>(&HookKeyState), &o_keystate);
    ok += Hook(L"user32", "GetKeyboardState", reinterpret_cast<void*>(&HookKbState), &o_kbstate);
    ok += Hook(L"user32", "GetCursorPos", reinterpret_cast<void*>(&HookCursor), &o_cursor);
    if (GetModuleHandleW(L"xinput9_1_0"))
        ok += Hook(L"xinput9_1_0", "XInputGetState", reinterpret_cast<void*>(&HookXInput), &o_xinput);
    else if (g_log)
        g_log("[teclado] xinput9_1_0 no esta cargado");
    HookXInputLate();
    return ok;
}

void InputSetCapture(bool on) {
    if (on == g_capture.load()) return;
    if (on) {
        HookWindowThread();
        HookXInputLate();
        g_n.Reset();
        g_raw_kbd.store(false);
        if (o_cursor) o_cursor(&g_frozen_cursor);
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
    if (!on && g_log)  // que llego mientras el panel estuvo abierto (para saber por donde lee el juego)
        g_log("[teclado] durante el panel: WM_INPUT teclado=%u raton=%u otro=%u, WM_KEYDOWN=%u WM_KEYUP=%u "
              "WM_CHAR=%u raton(msg)=%u, GetRawInputData=%u GetAsyncKeyState=%u GetKeyState=%u "
              "GetKeyboardState=%u GetCursorPos=%u XInput=%u, teclas para el panel=%u",
              g_n.wm_input_kbd.load(), g_n.wm_input_mouse.load(), g_n.wm_input_other.load(), g_n.keydown.load(),
              g_n.keyup.load(), g_n.chars.load(), g_n.mouse_msgs.load(), g_n.raw_calls.load(),
              g_n.async_calls.load(), g_n.keystate_calls.load(), g_n.kbstate_calls.load(), g_n.cursor_calls.load(),
              g_n.xinput_calls.load(), g_n.events.load());
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
