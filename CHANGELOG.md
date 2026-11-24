# Changelog / Registro de cambios

## Sin publicar / Unreleased

**Español**
- Ruta nueva `PUT /forget?lat=&lon=` (en el DLL y en la versión Python), base para la lista de destinos: quita un punto de la lista y, si es el destino del planeta actual, también el destino y su marcador.
- Versión Python (laboratorio): con la partida ya cargada al inyectar NMS.py, el mod ya no toma un puntero equivocado; lee `cGcApplication::Data` del mismo global que usa el juego en `cGcApplication::Update` (NMS 185034) y lo comprueba antes de usarlo.
- Instrucciones: nota para laptops (si F9 abre otra cosa, usar Fn + F9, el bloqueo de Fn u otra tecla).
- **Idioma ruso** en el recuadro, el DLL (mensajes, puntos cardinales С/СВ/В…, recuadro dentro del juego) y la versión Python. Se elige en el menú "Idioma / Language / Язык"; con Windows en ruso es el idioma por defecto. En ruso el recuadro usa Consolas (Share Tech Mono no trae cirílico). Los agradecimientos, la postura sobre los mods y la divulgación de IA se muestran en inglés hasta tener su versión rusa aprobada. La ventana de NMS.py sigue en inglés en ruso (no trae letras cirílicas).

**English**
- New `PUT /forget?lat=&lon=` route (DLL and Python version), groundwork for the destinations list: removes a point from the list and, if it is the current planet's destination, also the destination and its marker.
- Python (lab) version: when NMS.py is injected with a save already loaded, the mod no longer picks a wrong pointer; it reads `cGcApplication::Data` from the same global the game uses in `cGcApplication::Update` (NMS 185034) and checks it before use.
- Instructions: laptop note (if F9 opens something else, use Fn + F9, the Fn lock or another key).
- **Russian language** in the overlay, the DLL (messages, cardinal points С/СВ/В…, in-game box) and the Python version. Pick it in the "Idioma / Language / Язык" menu; it is the default when Windows is in Russian. In Russian the overlay uses Consolas (Share Tech Mono has no Cyrillic). The acknowledgements, the stance on mods and the AI disclosure are shown in English until an approved Russian version exists. The NMS.py window stays in English when Russian is chosen (it has no Cyrillic glyphs).

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
- v0.2 en desarrollo (todo dentro del juego, sin `.exe`):
  - **0.2.0-dev.1:** sonda de Vulkan, solo diagnóstico. Probada en las dos PC de MightyZeus: todas las imágenes pasan por `vkQueuePresentKHR` pedido con `vkGetDeviceProcAddr`, en una sola cola.
  - **0.2.0-dev.2:** recuadro de prueba dentro del juego, arriba a la izquierda: título, coordenadas (o "esperando al juego"), y destino con distancia y rumbo. Se arma en la CPU y se copia en cada imagen antes de que el juego la muestre; sin sombreadores. Si algo no cuadra (otra cadena de imágenes, formato raro, la GPU ocupada), el juego presenta su imagen sin cambios. Se apaga con `"ingame_box": false` en los ajustes de `datos.json`. Además, las 9 funciones del juego se buscan con **patrones propios** (`dll/tools/sigs_185034.json`, sacados del NMS.exe con análisis estático), ya no con los de NMS.py; el constructor del marcador ahora apunta a la función real.
  - **0.2.0-dev.3 (sin probar en el juego):** el recuadro dentro del juego vuelve al estilo del recuadro `.exe`: números en DSEG7 Classic y texto en Share Tech Mono (incluidas en el DLL, solo para el juego; en ruso, Consolas). En una estación espacial, en lugar de "no estás en un planeta", muestra el nombre de la estación y debajo, más pequeño, el sistema y la galaxia. Volando en el espacio, lejos de un planeta, muestra el sistema y debajo la galaxia. Los nombres los da el propio juego (dos funciones opcionales con patrones propios; se piden solo al cambiar de sistema y cada 3 s); la galaxia sale de una tabla de 256 nombres. Si esas funciones no aparecen, se ve "Estación espacial" y la galaxia. Se apaga con `"location_names": false` en `datos.json`. Lo mismo en la versión Python y en el recuadro `.exe`.
  - **0.2.0-dev.4 (sin probar en el juego):** primer paso para quitar el `.exe`: **panel dentro del juego** con el teclado. F9 (o la tecla elegida) lo abre y lo cierra. Página **Destino**: escribes `lat, lon [nombre]` y Enter fija el destino; con ↑↓ eliges uno de los 10 guardados, Enter va a ese punto y Supr lo quita de la lista. Página **Ajustes** (Tab): idioma y tecla del panel (←→, al momento) y quitar el destino de este planeta. Esc cierra. Mientras el panel está abierto, el juego no recibe teclado, ratón ni mando (la nave no se mueve al escribir); las teclas que sueltas sí le llegan, para que nada quede presionado. Si el juego pierde el foco (Alt+Tab), el panel se cierra. Con `"ingame_box": false`, F9 sigue abriendo el recuadro `.exe` como antes.
  - **0.2.0-dev.5 (sin probar en el juego):** arreglo del panel de la dev.4. En el juego real se abría pero no recibía teclas (Esc no lo cerraba) y el ratón seguía moviendo la cámara: NMS usa SDL2, que lee el teclado como mensajes de la ventana, y la dev.4 solo miraba Raw Input. Ahora el panel filtra la cola de mensajes del hilo de la ventana del juego (solo ese hilo, nada global): teclas, letras, movimiento y clics del ratón van al panel y el juego no los ve. También deja quieto el cursor (GetCursorPos) y bloquea el mando por xinput1_4/1_3 (que carga SDL). Al cerrar el panel, el registro anota qué llegó por cada camino.
  - **0.2.0-dev.6 (sin probar en el juego):** según la prueba de la dev.5 (el teclado y el ratón ya funcionan en el panel). **Lista por planeta:** cada destino guardado recuerda su planeta y la lista solo muestra los del planeta donde estás (10 por planeta). Los de antes (sin planeta) toman el planeta de su destino si coincide; los demás se conservan en `datos.json` pero no se muestran. También en la versión Python y en el recuadro `.exe`. **Recuadro dentro del juego:** coordenadas del destino en azul debajo de las tuyas y, debajo, nombre, distancia y rumbo; **flecha 3D** a la derecha (la misma del `.exe`; sin destino, la esfera en reposo); el ancho se ajusta al texto; **tamaño Normal / Grande** en Ajustes del panel (también agranda el panel).
  - **0.2.0-dev.7 (sin probar en el juego):** según la prueba de la dev.6. **Tamaños iguales al recuadro `.exe`:** los mismos puntos (Normal 8/6, Grande 15/10) pasados a píxeles con el DPI de Windows, como hacía el `.exe` (antes se escalaba con la resolución y el Normal salía como el Grande del `.exe`). Mismo orden y textos: título, fila AHORA, fila DESTINO con el nombre al lado y debajo "a X km · rumbo N° NE". **Flecha:** antes medía la línea recta al destino; a cientos de km esa línea pasa por dentro del planeta y la flecha apuntaba hacia ti, y la inclinación del cuerpo o de la nave la torcía. Ahora el giro se mide en el plano del suelo y la inclinación como si el suelo fuera plano (lejos queda casi plana; encima del destino apunta hacia abajo), y se mueve suave como en el `.exe`. También en la versión Python y en el recuadro `.exe`.

**English**
- Step 1: overlay as an `.exe` (PyInstaller), built on GitHub Actions. Tested by MightyZeus.
- Step 2 (v0.1.0-dll.3; dll.2 did not find the application in the real game): `winmm.dll` next to `NMS.exe`. Takes `cGcApplication` from `cTkFSM::StateChange`, like NMS.py. Reads the player's position and planet (read only) and serves the coordinates to the overlay on `127.0.0.1:5000`, using the same `datos.json` as the Python version. Saved destinations, distance, bearing, arrow and arrival at 50 m. If the NMS.exe version doesn't match, the DLL disables itself. Tested in the real game by MightyZeus (Nov 17, 2026): live coordinates OK.
- Step 3 (v0.1.0-dll.4, tested in game): set and clear the destination from the overlay, history of 10, in-game HUD marker (`cGcBaseBuildingManager::AddHUDMarker`, the same BaseBuildingHUD method as the Python version; requested every frame and never stored in the save), configurable hotkey (F9 by default, F5–F11 or none) that takes effect immediately, no "Reload Mod". After setting a destination, focus returns to the game.
- v0.2 in development (everything inside the game, no `.exe`):
  - **0.2.0-dev.1:** Vulkan probe, diagnostics only. Tested on both of MightyZeus's PCs: every frame goes through `vkQueuePresentKHR` obtained with `vkGetDeviceProcAddr`, on a single queue.
  - **0.2.0-dev.2:** test box inside the game, top left: title, coordinates (or "waiting for the game"), and destination with distance and bearing. Built on the CPU and copied into each frame before the game shows it; no shaders. If anything doesn't fit (another swapchain, an unusual format, the GPU busy), the game presents its frame unchanged. Turn it off with `"ingame_box": false` in the `datos.json` settings. Also, the game's 9 functions are now found with **our own patterns** (`dll/tools/sigs_185034.json`, taken from NMS.exe by static analysis) instead of NMS.py's; the marker constructor now points to the real function.
  - **0.2.0-dev.3 (not tested in game yet):** the in-game box goes back to the `.exe` box style: numbers in DSEG7 Classic and text in Share Tech Mono (embedded in the DLL, for the game only; Consolas in Russian). In a space station, instead of "you're not on a planet", it shows the station name and below it, smaller, the system and the galaxy. Flying in space, away from planets, it shows the system and below it the galaxy. The names come from the game itself (two optional functions with our own patterns; asked for only when the system changes and every 3 s); the galaxy comes from a table of 256 names. If those functions aren't found, it shows "Space station" and the galaxy. Turn it off with `"location_names": false` in `datos.json`. Same in the Python version and the `.exe` box.
  - **0.2.0-dev.4 (not tested in game yet):** first step to drop the `.exe`: an **in-game panel** driven by the keyboard. F9 (or the chosen key) opens and closes it. **Destination** page: type `lat, lon [name]` and Enter sets the destination; ↑↓ picks one of the 10 saved ones, Enter goes there and Del removes it from the list. **Settings** page (Tab): language and panel key (←→, immediately) and clear this planet's destination. Esc closes. While the panel is open, the game gets no keyboard, mouse or gamepad input (the ship doesn't move while you type); released keys do reach it, so nothing stays pressed. If the game loses focus (Alt+Tab), the panel closes. With `"ingame_box": false`, F9 opens the `.exe` box as before.
  - **0.2.0-dev.5 (not tested in game yet):** fix for the dev.4 panel. In the real game it opened but got no keys (Esc didn't close it) and the mouse still moved the camera: NMS uses SDL2, which reads the keyboard as window messages, and dev.4 only looked at Raw Input. Now the panel filters the message queue of the game window's thread (that thread only, nothing global): keys, characters, mouse movement and clicks go to the panel and the game doesn't see them. It also keeps the cursor still (GetCursorPos) and blocks the gamepad through xinput1_4/1_3 (loaded by SDL). When the panel closes, the log records what arrived through each path.
  - **0.2.0-dev.6 (not tested in game yet):** based on the dev.5 test (keyboard and mouse now work in the panel). **Per-planet list:** each saved destination remembers its planet and the list only shows the ones for the planet you're on (10 per planet). Older ones (no planet) take the planet of their destination if it matches; the rest stay in `datos.json` but aren't shown. Also in the Python version and the `.exe` box. **In-game box:** destination coordinates in blue below yours and, below that, name, distance and bearing; **3D arrow** on the right (same as the `.exe`; no destination, the idle sphere); the width fits the text; **Normal / Large size** in the panel Settings (also enlarges the panel).
  - **0.2.0-dev.7 (not tested in game yet):** based on the dev.6 test. **Sizes match the `.exe` box:** the same points (Normal 8/6, Large 15/10) turned into pixels with the Windows DPI, as the `.exe` did (before it scaled with resolution and Normal came out like the `.exe`'s Large). Same order and texts: title, NOW row, DEST row with the name next to it and below "X away · bearing N° NE". **Arrow:** it used the straight line to the destination; hundreds of km away that line goes through the planet and the arrow pointed at you, and body or ship tilt twisted it. Now the turn is measured on the ground plane and the tilt as if the ground were flat (far away it's almost flat; above the destination it points down), and it moves smoothly like in the `.exe`. Also in the Python version and the `.exe` box.

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
