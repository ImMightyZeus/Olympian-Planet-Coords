// Olympian Planet Coords — lectura segura de la memoria del juego (solo lectura).
// Olympian Planet Coords — safe reading of the game's memory (read only).
//
// Olympian Planet Coords — Copyright © 2026 MightyZeus · GPL-3.0 License
//
// Sigue los mismos punteros que NMS.py: cGcApplication -> mpData -> mSimulation -> mEnvironment ->
// mPlayerEnvironment, y mGameState -> mPlayerState -> mLocation. Cada lectura usa
// ReadProcessMemory sobre el propio proceso: si un puntero todavia no es valido (pantalla de carga,
// menu), la lectura falla sin romper el juego. Nunca escribe en la memoria del juego.
#pragma once

#include <cstdint>

#include "core.hpp"

namespace olympian {

bool ReadGameView(uintptr_t application, GameView* out);

// true si la vista parece de verdad del jugador: ubicacion valida y una matriz de orientacion con
// vectores unitarios. Sirve para reconocer cual de los objetos cTkFSM es cGcApplication.
bool PlausibleView(const GameView& v);

// Lee un puntero (8 bytes) de forma segura; 0 si no se puede.
uintptr_t ReadPointer(uintptr_t addr);

}  // namespace olympian
