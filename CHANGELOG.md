# Changelog / Registro de cambios

## v0.1.0-beta — 2026-11-17

**Español**
- **Primera versión para jugadores (DLL):** `winmm.dll` en la carpeta `Binaries` del juego y el recuadro como programa (`OlympianPlanetCoords-Overlay.exe`, con Pillow incluido). Sin Python, NMS.py ni comandos: el juego se abre desde Steam.
- Hace lo mismo que la v0.0.3: coordenadas en vivo, destino por planeta con marcador en el HUD, distancia y rumbo, flecha 3D, llegada a 50 m, historial de 10, tamaño, idioma.
- La tecla (F9 por defecto) se cambia al momento, sin "Reload Mod". Después de poner un destino, el foco vuelve al juego.
- Mismo `datos.json` que la versión Python: se conservan destinos, historial y ajustes.
- Solo para NMS build 185034: con otra versión el DLL se desactiva solo y el juego funciona normal.
- El recuadro dice "Esperando al mod (¿juego abierto?)" en lugar de "Esperando a NMS.py".
- Instrucciones nuevas para la versión DLL; las de NMS.py pasan a `mod/INSTRUCCIONES_NMSPY.md`.
- Probada por MightyZeus en el juego real (dll.3 y dll.4, 17-nov-2026).

**English**
- **First version for players (DLL):** `winmm.dll` in the game's `Binaries` folder and the box as a program (`OlympianPlanetCoords-Overlay.exe`, with Pillow included). No Python, NMS.py or commands: launch the game from Steam.
- Same features as v0.0.3: live coordinates, per-planet destination with a HUD marker, distance and bearing, 3D arrow, arrival at 50 m, history of 10, size, language.
- The hotkey (F9 by default) changes immediately, no "Reload Mod". After setting a destination, focus returns to the game.
- Same `datos.json` as the Python version: destinations, history and settings are kept.
- Only for NMS build 185034: with any other version the DLL disables itself and the game runs normally.
- The box says "Waiting for the mod (is the game running?)" instead of "Waiting for NMS.py".
- New instructions for the DLL version; the NMS.py ones move to `mod/INSTRUCTIONS_NMSPY.md`.
- Tested by MightyZeus in the real game (dll.3 and dll.4, Nov 17, 2026).

## Desarrollo del DLL / DLL development

**Español**
- Paso 1: overlay como `.exe` (PyInstaller), compilado en GitHub Actions. Probado por MightyZeus.
- Paso 2 (v0.1.0-dll.3; la dll.2 no encontraba la aplicación en el juego real): `winmm.dll` junto a `NMS.exe`. Toma `cGcApplication` de `cTkFSM::StateChange`, como NMS.py. Lee la posición del jugador y el planeta (solo lectura) y sirve las coordenadas al overlay en `127.0.0.1:5000`, con el mismo `datos.json` que la versión Python. Destinos guardados, distancia, rumbo, flecha y llegada a 50 m. Si la versión de NMS.exe no coincide, el DLL se desactiva solo. Probado en el juego real por MightyZeus (17-nov-2026): coordenadas en vivo OK.
- Paso 3 (v0.1.0-dll.4, probada en el juego): poner y quitar destino desde el overlay, historial de 10, marcador en el HUD del juego (`cGcBaseBuildingManager::AddHUDMarker`, el mismo método BaseBuildingHUD de la versión Python; se pide en cada frame y no queda guardado en la partida), tecla configurable (F9 por defecto, F5–F11 o ninguna) que se activa al momento, sin "Reload Mod". Después de poner un destino, el foco vuelve al juego.

**English**
- Step 1: overlay as an `.exe` (PyInstaller), built on GitHub Actions. Tested by MightyZeus.
- Step 2 (v0.1.0-dll.3; dll.2 did not find the application in the real game): `winmm.dll` next to `NMS.exe`. Takes `cGcApplication` from `cTkFSM::StateChange`, like NMS.py. Reads the player's position and planet (read only) and serves the coordinates to the overlay on `127.0.0.1:5000`, using the same `datos.json` as the Python version. Saved destinations, distance, bearing, arrow and arrival at 50 m. If the NMS.exe version doesn't match, the DLL disables itself. Tested in the real game by MightyZeus (Nov 17, 2026): live coordinates OK.
- Step 3 (v0.1.0-dll.4, tested in game): set and clear the destination from the overlay, history of 10, in-game HUD marker (`cGcBaseBuildingManager::AddHUDMarker`, the same BaseBuildingHUD method as the Python version; requested every frame and never stored in the save), configurable hotkey (F9 by default, F5–F11 or none) that takes effect immediately, no "Reload Mod". After setting a destination, focus returns to the game.

## v0.0.3 — 2026-11-16

**Español**
- Tecla por defecto F9 (F8 la usa Imperial Galactic Map). Configurable: F5–F11 o ninguna, desde el menú del recuadro o la ventana de NMS.py; se activa con "Reload Mod".
- Ya no se queda en "esperando al juego" si NMS.py se inyecta con la partida cargada.
- Flecha con bordes suaves (Pillow a 4× y ventana con transparencia real por píxel). Sin Pillow se usa la flecha anterior.
- Español e inglés: menú "Idioma / Language" en el recuadro (por defecto, el idioma de Windows); mensajes del mod y ventana de NMS.py traducidos.
- El encabezado del recuadro muestra el nombre del mod en lugar de "COORDENADAS"; icono del mod en las ventanas.
- Ventana "Acerca de" en dos columnas con logo, licencia GPL-3.0, agradecimientos, lo que hace y lo que no hace, divulgación sobre el uso de IA y otros proyectos.
- Instrucciones: carpeta por versión, MODS del juego vacía, orden de arranque y "start paused" en No.

**English**
- Default hotkey is now F9 (F8 is used by Imperial Galactic Map). Configurable: F5–F11 or none, from the box menu or the NMS.py window; takes effect after "Reload Mod".
- No longer stuck on "waiting for the game" if NMS.py is injected with a save already loaded.
- Smooth-edged arrow (Pillow at 4× and a window with real per-pixel transparency). Without Pillow the previous arrow is used.
- Spanish and English: "Idioma / Language" menu in the box (defaults to the Windows language); mod messages and NMS.py window translated.
- The box header shows the mod's name instead of "COORDENADAS"; mod icon on the windows.
- Two-column About window with logo, GPL-3.0 license, acknowledgements, what it does and doesn't do, AI disclosure and other projects.
- Instructions: one folder per version, game's MODS folder empty, startup order and "start paused" set to No.

## v0.0.2 — 2026-11-16

**Español:** tamaño Normal/Grande; flecha 3D ámbar con círculo dorado oscuro que se convierte en esfera en reposo; errores persistentes; textos cortos en la ventana de NMS.py; opción "Flecha inv.".

**English:** Normal/Large size; amber 3D arrow with a dark-gold ring that turns into a sphere when idle; persistent errors; short labels in the NMS.py window; "Flecha inv." (invert arrow) option.

## v0.0.1 — 2026-11

**Español:** primera versión: coordenadas en vivo, destino por planeta con marcador en el HUD, distancia y rumbo, llegada a 50 m, historial de 10.

**English:** first version: live coordinates, per-planet destination with HUD marker, distance and bearing, arrival at 50 m, history of 10.
