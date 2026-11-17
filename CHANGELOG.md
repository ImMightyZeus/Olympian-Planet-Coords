# Changelog / Registro de cambios

## En desarrollo / In development — DLL

**Español**
- Paso 1: overlay como `.exe` (PyInstaller), compilado en GitHub Actions. Probado por MightyZeus.
- Paso 2 (v0.1.0-dll.2, en prueba): `winmm.dll` junto a `NMS.exe`. Lee la posición del jugador y el planeta (solo lectura) y sirve las coordenadas al overlay en `127.0.0.1:5000`, con el mismo `datos.json` que la versión Python. Destinos guardados, distancia, rumbo, flecha y llegada a 50 m. Si la versión de NMS.exe no coincide, el DLL se desactiva solo.

**English**
- Step 1: overlay as an `.exe` (PyInstaller), built on GitHub Actions. Tested by MightyZeus.
- Step 2 (v0.1.0-dll.2, in testing): `winmm.dll` next to `NMS.exe`. Reads the player's position and planet (read only) and serves the coordinates to the overlay on `127.0.0.1:5000`, using the same `datos.json` as the Python version. Saved destinations, distance, bearing, arrow and arrival at 50 m. If the NMS.exe version doesn't match, the DLL disables itself.

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
