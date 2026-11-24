// Olympian Planet Coords — teclado dentro del juego: mientras el panel esta abierto, las teclas y el
// raton son para el panel y el juego no los recibe (la nave no se mueve al escribir).
// Olympian Planet Coords — in-game keyboard: while the panel is open, keys and mouse go to the panel
// and the game doesn't get them (the ship doesn't move while you type).
//
// Olympian Planet Coords — Copyright © 2026 MightyZeus · GPL-3.0 License
//
// NMS.exe 185034 usa SDL2 (incluido en el .exe): el teclado llega como mensajes de la ventana
// (WM_KEYDOWN, WM_CHAR), el raton como WM_INPUT / WM_MOUSEMOVE, y ademas consulta GetAsyncKeyState,
// GetKeyState, GetKeyboardState, GetCursorPos y XInput (mando). La dev.4 solo miraba GetRawInputData y
// en el juego real no recibia nada. Ahora, mientras el panel captura:
//  * Cola de mensajes del hilo de la ventana (WH_GETMESSAGE, solo ese hilo, nada global): las teclas
//    (WM_KEYDOWN o WM_INPUT de teclado) van al panel y el juego recibe WM_NULL. Tambien WM_CHAR, el
//    movimiento y los clics del raton. Los botones que se SUELTAN si le llegan.
//  * Soltar una tecla: si se presiono para el panel, el juego tampoco ve que se suelta (aunque el panel
//    ya se haya cerrado: asi Esc o Enter no abren el menu del juego). Si se presiono ANTES de abrir el
//    panel (por ejemplo, caminar), soltarla si le llega, para que nada quede "pegado".
//  * GetAsyncKeyState / GetKeyState / GetKeyboardState: "nada presionado" (y hasta soltar las teclas
//    del panel, maximo 2 s despues de cerrarlo). GetCursorPos: el cursor queda quieto.
//  * GetRawInputData (respaldo): teclas y raton vacios. XInputGetState (9_1_0, 1_4, 1_3 y el Ex de
//    SDL): mando en reposo.
// Al cerrar el panel se anota en el registro que llego por cada camino, para ver por donde lee el juego.
// Sin capturar, todo pasa tal cual.
#pragma once

#include <windows.h>

#include <vector>

#include "ingame_ui.hpp"

namespace olympian {

using InputLogFn = void (*)(const char* fmt, ...);

// Instala los enganches (MinHook ya inicializado). Devuelve cuantos de los 6 quedaron puestos.
int InputHookInstall(InputLogFn log);
// Empieza o deja de capturar. Al empezar, engancha la ventana del juego (la que tiene el foco).
void InputSetCapture(bool on);
bool InputCapturing();
// Una vez por frame (hilo del juego): limpia teclas del panel que se quedaron sin soltar.
void InputTick();
// Teclas que llegaron para el panel desde la ultima vez (se llama desde el hilo del juego).
void InputDrain(std::vector<UiEvent>* out);
// GetAsyncKeyState verdadero (sin el filtro de la captura), para la tecla del panel.
SHORT InputRealAsyncKeyState(int vk);

// Partes sin enganches, para las pruebas:
// Traduce una tecla de Raw Input a eventos del panel (state: teclas de mayusculas, AltGr... que se van
// llevando). Devuelve true si la tecla se presiono (no si se solto).
struct KeyTracker {
    BYTE state[256] = {};
    void Reset(bool caps, bool num);
};
bool TranslateKey(const RAWKEYBOARD& k, KeyTracker* t, HKL layout, std::vector<UiEvent>* out);
// Deja el dato de Raw Input "vacio" para el juego. Devuelve true si cambio algo.
bool NeutralizeRaw(RAWINPUT* ri);

}  // namespace olympian
