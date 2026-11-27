# Olympian Planet Coords — v0.2.0-beta

```
Olympian Planet Coords — Copyright © 2026 MightyZeus · Licencia GPL-3.0
Apoya el proyecto: https://paypal.me/ImMightyZeus · Repositorio: https://github.com/ImMightyZeus/Olympian-Planet-Coords

Agradecimientos:
A mi querida esposa y mi amada familia, por permitirme seguir siendo un niño aun en mi adultez.
Pangro Daer Barun D'Gross, por seguirme corrompiendo con videojuegos.
Fenrir, porque aunque ya casi no juega, aún se presta a conversar sobre mis aventuras y desventuras.
```

Desarrollado por **MightyZeus**, originario de Nicaragua. *English: [INSTRUCTIONS.md](INSTRUCTIONS.md).*

> **Versión beta.** Un DLL que se copia junto al juego. Desde esta versión todo se ve y se maneja **dentro del juego**: el recuadro `.exe` es opcional. No necesita Python ni NMS.py. Si encuentras algún detalle, repórtalo (ver al final).

---

Mod para No Man's Sky (PC, Steam, Windows):

1. **Coordenadas en tiempo real** en un recuadro dentro del juego, arriba a la izquierda (a pie, en nave, en vehículo y en órbita).
2. **F9** abre el **panel** dentro del juego: escribe `lat, lon [nombre]` → **Enter** = destino con marcador en el HUD del juego. Mientras el panel está abierto, el juego no recibe teclado, ratón ni mando.
3. **Coordenadas del destino, distancia y rumbo**, y una **flecha 3D** que apunta hacia él.
4. Al llegar (50 m o menos) el destino se quita solo; la flecha se convierte en una esfera.
5. **Un destino por planeta**, recordado aunque te vayas; solo aparece en ese planeta.
6. **Destinos guardados de cada planeta** (hasta 10): en el panel, ↑ ↓ para elegir, **Enter** para ir, **Supr** para quitar.
7. **Fuera de los planetas:** en una estación, su nombre y debajo sistema y galaxia; volando en el espacio, el sistema y debajo la galaxia.
8. **Tamaño** Normal / Grande, **tecla** configurable e **idioma** español / inglés / ruso (en el panel: **Tab** → Ajustes).

## Novedades de la v0.2.0-beta

- **Todo dentro del juego:** recuadro y panel dibujados dentro del juego (letras DSEG7 y Share Tech Mono, como el `.exe`). Ya no hace falta el modo sin bordes ni el `.exe`.
- **Panel con el teclado** (F9): destino, lista de destinos de este planeta, ajustes.
- **Lista por planeta:** los destinos guardados solo aparecen en su planeta.
- **Nombres de estación, sistema y galaxia** cuando no estás en un planeta.
- **Flecha corregida:** antes, con destinos lejanos, apuntaba hacia ti; ahora apunta sobre el suelo, hacia donde queda el destino.
- **Para la actualización del juego** NMS.exe 186923.
- Idioma **ruso**.

## Contenido del paquete

```
Binaries\winmm.dll                 ← el mod (va en la carpeta Binaries del juego)
OlympianPlanetCoords-Overlay.exe   ← opcional: el recuadro fuera del juego, como en la v0.1.0
INSTRUCCIONES.md / INSTRUCTIONS.md
LICENSE                            ← GNU GPL v3
THIRD_PARTY_LICENSES\              ← licencias de los componentes de terceros
SHA256SUMS.txt                     ← huellas para comprobar los archivos
```

## Instalación (una sola vez)

1. En Steam: clic derecho en **No Man's Sky** → **Administrar** → **Ver archivos locales**. Se abre la carpeta del juego.
2. Copia `Binaries\winmm.dll` del paquete a la carpeta **`Binaries`** del juego (la que contiene `NMS.exe`).
   - Si Windows dice que ya existe un `winmm.dll` ahí, **no lo reemplances**: es de otro mod. Avísanos.
   - Funciona junto a Imperial Galactic Map (IGM usa otro archivo, `xinput9_1_0.dll`).
3. Listo. El `.exe` del recuadro es **opcional** (para quien prefiera el recuadro fuera del juego, en modo sin bordes). La primera vez que lo abras, Windows puede mostrar "Windows protegió su PC" (SmartScreen) porque todavía no tiene firma digital: **Más información** → **Ejecutar de todas formas**. Puedes comparar su huella con `SHA256SUMS.txt` (en PowerShell: `Get-FileHash OlympianPlanetCoords-Overlay.exe`).

## Cada vez que juegues

1. Abre **No Man's Sky desde Steam**, como siempre.
2. Carga tu partida. En unos segundos aparece el recuadro con las coordenadas, arriba a la izquierda.

## Uso

- **F9** abre y cierra el panel. **Esc** lo cierra. **Tab** cambia entre **Destino** y **Ajustes**.
- **Destino:** escribe y pulsa **Enter**. Formatos: `+12.34, -56.78` · `12.34 -56.78 Mi base` · `12,34; -56,78; Mi base` (coma decimal → `;`). Con **↑ ↓** eliges un destino guardado de este planeta: **Enter** va a ese punto, **Supr** lo quita de la lista.
- **Ajustes:** con **↑ ↓** eliges y con **← →** cambias: idioma, tecla del panel (F5 a F11), tamaño del recuadro (Normal / Grande) y "Quitar el destino de este planeta" (**Enter**).
- Mientras el panel está abierto, el juego no recibe teclado, ratón ni mando: tu personaje y tu nave no se mueven al escribir. Si cambias de ventana (Alt+Tab), el panel se cierra.
- **En laptops:** si al pulsar F9 se abre otra cosa (el navegador, el brillo, el volumen…), es la función especial de esa tecla en tu teclado. Usa **Fn + F9**, activa el bloqueo de Fn de tu laptop o elige otra tecla en Ajustes.
- El rumbo es respecto al norte del planeta. La flecha es respecto a hacia donde miras (tu personaje a pie o el frente de tu nave): de frente, el destino está delante.
- Si alguna vez quieres apagar el recuadro dentro del juego (y volver al `.exe`): en `%LOCALAPPDATA%\OlympianPlanetCoords\datos.json`, dentro de `"settings"`, pon `"ingame_box": false`.

## Versión del juego

Esta beta funciona con **No Man's Sky build 186923** (la versión del archivo `NMS.exe`). Cuando el juego se actualice, el DLL lo detecta y **se desactiva solo**: el juego funciona normal, sin el recuadro. En ese caso, espera la versión del mod para ese parche.

## Desinstalar

Borra `winmm.dll` de la carpeta `Binaries` del juego (y la carpeta `Binaries\Olympian`, que guarda el registro). Tus destinos están en `%LOCALAPPDATA%\OlympianPlanetCoords\`; bórrala también si no piensas volver.

**No uses a la vez la versión DLL y la de laboratorio con NMS.py**: las dos usan el puerto 5000.

## Si algo falla, incluye esto al reportarlo (Issues de GitHub)

1. El registro del mod: `Binaries\Olympian\olympian.log` en la carpeta del juego (si Windows no deja escribir ahí: `%LOCALAPPDATA%\OlympianPlanetCoords\olympian.log`).
2. `http://localhost:5000/OlympianPlanetCoords/debug` (en el navegador, con el juego abierto y estando en un planeta).
3. Qué estabas haciendo y, si puedes, una captura.

## Lo que hace y lo que no hace Olympian Planet Coords

Todo mod o editor cambia el juego, y Olympian Planet Coords no es la excepción. Lo que lo distingue no es una superioridad moral, sino estos compromisos de seguridad:

1. **Solo presentará resultados que el juego podría producir por sí mismo**, siguiendo sus propias reglas y límites. Nunca dejará tu partida en un estado imposible.
2. **Solo modificará aquello que se sabe con certeza; no destruirá tus saves.** No aplicará funciones experimentales de alto riesgo: ya el riesgo lo corremos por ti, con pruebas reales continuamente monitoreadas. De todo lo demás podremos mostrarte una lectura, pero hasta no definir un método seguro, no ofreceremos edición.
3. **Tratará de mantener tu experiencia del juego lo más fiel posible** (sin dejar de ofrecer mejoras opcionales). Verás que ofrece herramientas para resolver problemas reales, no atajos para "tenerlo todo".
4. **Pondrá tu seguridad primero:** respaldos antes de cualquier cambio a tu partida, lo mínimo invasivo posible y avisos claros cuando algo tenga riesgo.
5. **Será transparente:** código abierto, sin relación con Hello Games, y explicará qué hace y qué no hace cada función.

En este mod: solo lee la posición del jugador y los nombres del sistema y de la estación, pone un marcador del HUD como los que el juego ya usa (el juego lo dibuja y se vuelve a pedir en cada frame) y dibuja su recuadro sobre la imagen del juego. No modifica la partida guardada, el inventario ni nada que se guarde en el save. No toca archivos del juego: solo agrega `winmm.dll`, que se borra para desinstalar.

## Divulgación sobre el uso de IA

El uso de nuevas tecnologías forma parte del día a día de este y de otros proyectos de MightyZeus. Olympian Planet Coords se desarrolla con la ayuda de una IA (Claude), que participa en la escritura y revisión del código; la dirección del proyecto, qué funciones entran y cuáles se dejan fuera a propósito, y las pruebas en el juego real son trabajo humano.
El compromiso de fondo no cambia: crear soluciones y herramientas reales desde la perspectiva de un jugador apasionado por disfrutar del juego y por ayudar a otros a hacerlo.

## Licencias de terceros

Textos completos en `THIRD_PARTY_LICENSES\`.

- Olympian Planet Coords: GNU GPL v3.0 (archivo LICENSE).
- MinHook — Tsuda Kageyu, licencia BSD-2-Clause (dentro del DLL).
- JSON for Modern C++ (nlohmann/json) — Niels Lohmann, licencia MIT (dentro del DLL).
- Posiciones de los datos del juego tomadas de NMS.py / pyMHF — monkeyman192, licencia MIT (los patrones de funciones son propios).
- Dentro del `.exe` (opcional): Python (licencia PSF), Pillow (licencia MIT-CMU) y el cargador de PyInstaller (GPL con excepción).
- Fuentes DSEG7 Classic (keshikan) y Share Tech Mono (Carrois Type Design) — SIL Open Font License 1.1 (dentro del DLL y del `.exe`).

## Otros proyectos de MightyZeus para No Man's Sky

- **Olympian NMS Save Editor**: editor de partidas seguro y visual (PC/Steam). https://github.com/ImMightyZeus/Olympian-NMS-Save-Editor

Todos los proyectos: https://github.com/ImMightyZeus
