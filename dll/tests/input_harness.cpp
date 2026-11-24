// Olympian Planet Coords — banco de pruebas del teclado del panel (se corre con Wine).
// Prueba: traduccion de teclas de Raw Input, que el juego reciba teclas vacias, los enganches de
// GetAsyncKeyState / GetKeyState / GetKeyboardState y la ventana enganchada.
//
// Olympian Planet Coords — Copyright © 2026 MightyZeus · GPL-3.0 License
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "MinHook.h"
#include "input_hook.hpp"

using namespace olympian;

static int g_fail = 0;
#define CHECK(c)                                                       \
    do {                                                               \
        if (!(c)) {                                                    \
            std::printf("FALLA %s:%d: %s\n", __FILE__, __LINE__, #c); \
            ++g_fail;                                                  \
        }                                                              \
    } while (0)

static void Log(const char* fmt, ...) {
    va_list a;
    va_start(a, fmt);
    std::vprintf(fmt, a);
    va_end(a);
    std::printf("\n");
}

static RAWKEYBOARD Key(USHORT vk, bool up, USHORT make = 0, bool e0 = false) {
    RAWKEYBOARD k{};
    k.VKey = vk;
    k.MakeCode = make ? make : static_cast<USHORT>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
    k.Flags = static_cast<USHORT>((up ? RI_KEY_BREAK : RI_KEY_MAKE) | (e0 ? RI_KEY_E0 : 0));
    k.Message = up ? WM_KEYUP : WM_KEYDOWN;
    return k;
}

static std::u32string Type(KeyTracker* t, HKL hkl, const std::vector<RAWKEYBOARD>& keys, std::vector<UiEvent>* all) {
    std::u32string out;
    for (const auto& k : keys) {
        std::vector<UiEvent> ev;
        TranslateKey(k, t, hkl, &ev);
        for (auto& e : ev) {
            all->push_back(e);
            if (e.key == UiKey::Char) out += e.ch;
        }
    }
    return out;
}

static int g_orig_calls = 0, g_keyups = 0;
static LRESULT CALLBACK OrigProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_KEYDOWN || m == WM_CHAR || m == WM_LBUTTONDOWN) ++g_orig_calls;
    if (m == WM_KEYUP) ++g_keyups;
    return DefWindowProcW(h, m, w, l);
}

int main() {
    HKL us = LoadKeyboardLayoutW(L"00000409", 0);
    KeyTracker t;
    t.Reset(false, true);
    std::vector<UiEvent> all;
    // "12.5, -40 Ab" con Shift para la A
    std::vector<RAWKEYBOARD> keys = {
        Key('1', false), Key('1', true), Key('2', false), Key('2', true), Key(VK_OEM_PERIOD, false),
        Key(VK_OEM_PERIOD, true), Key('5', false), Key(VK_OEM_COMMA, false), Key(VK_SPACE, false),
        Key(VK_OEM_MINUS, false), Key('4', false), Key('0', false), Key(VK_SPACE, false),
        Key(VK_SHIFT, false, 0x2A), Key('A', false), Key(VK_SHIFT, true, 0x2A), Key('B', false),
        Key(VK_NUMPAD7, false),
    };
    std::u32string got = Type(&t, us, keys, &all);
    std::printf("escrito: ");
    for (char32_t c : got) std::printf("%c", static_cast<char>(c));
    std::printf("\n");
    CHECK(got == U"12.5, -40 Ab7");
    // Teclas de control
    all.clear();
    Type(&t, us,
         {Key(VK_BACK, false), Key(VK_RETURN, false), Key(VK_ESCAPE, false), Key(VK_UP, false, 0x48, true),
          Key(VK_DOWN, false, 0x50, true), Key(VK_LEFT, false, 0x4B, true), Key(VK_RIGHT, false, 0x4D, true),
          Key(VK_DELETE, false, 0x53, true), Key(VK_TAB, false), Key(VK_F9, false), Key(VK_RETURN, true)},
         &all);
    std::vector<UiKey> want = {UiKey::Back, UiKey::Enter, UiKey::Esc, UiKey::Up, UiKey::Down,
                               UiKey::Left, UiKey::Right, UiKey::Del,   UiKey::Tab};
    CHECK(all.size() == want.size());
    for (size_t i = 0; i < want.size() && i < all.size(); ++i) CHECK(all[i].key == want[i]);
    // Ctrl+C no escribe; Bloq Mayus si
    all.clear();
    got = Type(&t, us, {Key(VK_CONTROL, false, 0x1D), Key('C', false), Key(VK_CONTROL, true, 0x1D)}, &all);
    CHECK(got.empty());
    got = Type(&t, us, {Key(VK_CAPITAL, false), Key(VK_CAPITAL, true), Key('Z', false)}, &all);
    CHECK(got == U"Z");

    // Lo que le llega al juego
    RAWINPUT ri{};
    ri.header.dwType = RIM_TYPEKEYBOARD;
    ri.data.keyboard = Key('W', false);
    CHECK(NeutralizeRaw(&ri) && ri.data.keyboard.VKey == 0xFF && ri.data.keyboard.MakeCode == 0);
    ri.data.keyboard = Key('W', true);  // soltar: le llega tal cual (nada queda presionado)
    CHECK(!NeutralizeRaw(&ri) && ri.data.keyboard.VKey == 'W');
    ri.header.dwType = RIM_TYPEMOUSE;
    ri.data.mouse = RAWMOUSE{};
    ri.data.mouse.lLastX = 25;
    ri.data.mouse.lLastY = -7;
    ri.data.mouse.usButtonFlags = RI_MOUSE_LEFT_BUTTON_DOWN | RI_MOUSE_RIGHT_BUTTON_UP | RI_MOUSE_WHEEL;
    ri.data.mouse.usButtonData = 120;
    CHECK(NeutralizeRaw(&ri));
    CHECK(ri.data.mouse.lLastX == 0 && ri.data.mouse.lLastY == 0);
    CHECK(ri.data.mouse.usButtonFlags == RI_MOUSE_RIGHT_BUTTON_UP && ri.data.mouse.usButtonData == 0);

    // Enganches de verdad
    CHECK(MH_Initialize() == MH_OK);
    LoadLibraryW(L"xinput9_1_0.dll");
    int n = InputHookInstall(&Log);
    std::printf("enganches: %d\n", n);
    CHECK(n >= 4);
    // Ventana "del juego" en primer plano
    WNDCLASSW wc{};
    wc.lpfnWndProc = OrigProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"OlympianFakeGame";
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowW(L"OlympianFakeGame", L"juego", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 200, 100, nullptr,
                              nullptr, wc.hInstance, nullptr);
    SetForegroundWindow(hwnd);
    BYTE ks[256];
    std::memset(ks, 0, sizeof(ks));
    // Sin captura: pasa tal cual
    SendMessageW(hwnd, WM_KEYDOWN, 'W', 0);
    CHECK(g_orig_calls == 1);
    CHECK(GetKeyboardState(ks) != 0);
    InputSetCapture(true);
    CHECK(InputCapturing());
    CHECK(GetAsyncKeyState(VK_LBUTTON) == 0 && GetKeyState(VK_CAPITAL) == 0);
    std::memset(ks, 0xAB, sizeof(ks));
    CHECK(GetKeyboardState(ks) != 0);
    bool zero = true;
    for (BYTE b : ks) zero = zero && b == 0;
    CHECK(zero);
    if (GetForegroundWindow() == hwnd) {
        SendMessageW(hwnd, WM_KEYDOWN, 'W', 0);
        SendMessageW(hwnd, WM_CHAR, 'w', 0);
        SendMessageW(hwnd, WM_LBUTTONDOWN, 0, 0);
        CHECK(g_orig_calls == 1);  // no llegaron al juego
        SendMessageW(hwnd, WM_KEYUP, 'W', 0);  // soltar si llega (no se cuenta, pero no se bloquea)
    } else {
        std::printf("(Wine no dejo la ventana en primer plano: se omite la prueba de mensajes)\n");
    }
    InputSetCapture(false);
    if (GetForegroundWindow() == hwnd) {
        // Esc presionado con el panel abierto y soltado ya cerrado: el juego no ve ni lo uno ni lo otro
        g_keyups = 0;
        InputSetCapture(true);
        SendMessageW(hwnd, WM_KEYDOWN, VK_ESCAPE, 0);
        InputSetCapture(false);
        CHECK(GetAsyncKeyState(VK_LBUTTON) == 0);  // sigue filtrando hasta soltar
        SendMessageW(hwnd, WM_KEYUP, VK_ESCAPE, 0);
        CHECK(g_keyups == 0);
        SendMessageW(hwnd, WM_KEYUP, 'W', 0);  // tecla que no era del panel: le llega
        CHECK(g_keyups == 1);
        // Tecla del panel que nunca se suelta (Alt+Tab): a los 2 s se deja de filtrar
        InputSetCapture(true);
        SendMessageW(hwnd, WM_KEYDOWN, 'Q', 0);
        InputSetCapture(false);
        InputTick();
        BYTE kb[256];
        std::memset(kb, 0xAB, sizeof(kb));
        GetKeyboardState(kb);
        CHECK(kb[0] == 0);  // todavia filtra
        Sleep(2100);
        InputTick();
        std::memset(kb, 0xAB, sizeof(kb));
        GetKeyboardState(kb);
        CHECK(kb[0] != 0xAB);  // el original escribio el estado real
    }
    SendMessageW(hwnd, WM_KEYDOWN, 'W', 0);
    CHECK(g_orig_calls == 2 || GetForegroundWindow() != hwnd);
    std::vector<UiEvent> ev;
    InputDrain(&ev);
    CHECK(ev.empty());
    std::printf(g_fail ? "FALLOS: %d\n" : "OK teclado\n", g_fail);
    return g_fail ? 1 : 0;
}
