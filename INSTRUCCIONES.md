# Olympian Planet Coords — v0.0.3

```
Olympian Planet Coords — Copyright © 2026 MightyZeus · Licencia GPL-3.0
Apoya el proyecto: https://paypal.me/ImMightyZeus · Repositorio: https://github.com/ImMightyZeus/Olympian-Planet-Coords

Agradecimientos:
A mi querida esposa y mi amada familia, por permitirme seguir siendo un niño aun en mi adultez.
Pangro Daer Barun D'Gross, por seguirme corrompiendo con videojuegos.
Fenrir, porque aunque ya casi no juega, aún se presta a conversar sobre mis aventuras y desventuras.
```

Desarrollado por **MightyZeus**, originario de Nicaragua. *English: [INSTRUCTIONS.md](INSTRUCTIONS.md).*

> Estas instrucciones son para el **paquete de cada versión** (carpeta `OlympianPlanetCoords_vX.Y.Z`, con `GAMEDATA\MODS` y `OLYMPIAN_OVERLAY`). En el repositorio los mismos archivos están en `mod/` y `overlay/`.

---

Mod para No Man's Sky (PC, Steam, Windows):

1. **Coordenadas en tiempo real** arriba a la izquierda (a pie, en nave, en vehículo y en órbita).
2. **Clic** en el recuadro (o **F9** en el juego) → escribe `lat, lon [nombre]` → **Enter** = destino con marcador en el HUD.
3. **Distancia y rumbo** al destino, y una **flecha 3D** con bordes suaves que gira e inclina hacia él.
4. Al llegar (50 m o menos) el destino se quita solo; la flecha se convierte en una esfera y se reduce.
5. **Un destino por planeta**, recordado aunque te vayas; solo aparece en ese planeta.
6. **Historial** de los últimos 10 destinos (flechas ↑ ↓ en el campo de texto).
7. **Tamaño** Normal / Grande y **tecla** configurable (clic derecho en el recuadro).

## Novedades de la v0.0.3

- **Tecla F9 por defecto** (antes F8). F8 la usa Imperial Galactic Map para "agregar a favoritos". La tecla se cambia con clic derecho → **Tecla** (F5 a F11, o Ninguna) y se activa con **Reload Mod** en la ventana de NMS.py.
- **Ya no se queda en "esperando al juego"** si NMS.py se inicia con la partida ya cargada.
- **Flecha con bordes suaves**: se dibuja con Pillow y se muestra con transparencia real por píxel. Si Pillow no está instalado, se usa la flecha anterior.
- Ventana "Acerca de" con el repositorio, la divulgación sobre el uso de IA y otros proyectos.

## Contenido de la carpeta

```
GAMEDATA\MODS\OlympianPlanetCoords.py   ← el mod (NMS.py lo carga desde aquí)
OLYMPIAN_OVERLAY\                        ← el recuadro y la flecha
INSTRUCCIONES.md
```

**No copies nada dentro de la carpeta del juego.** NMS.py carga el mod desde esta carpeta. Si `GAMEDATA\MODS` del juego tiene archivos (aunque sea un `__pycache__`), NMS muestra el aviso "Mod Warning" al iniciar: esa carpeta del juego debe quedar vacía o no existir.

## Requisitos (una sola vez)

- Python **3.10 a 3.13** de python.org (la 3.14 aún no funciona con NMS.py). Para ver cuál tienes: `py --list`.
- NMS.py: `py -3.13 -m pip install -U nmspy` (cambia 3.13 por tu versión).
- **Nuevo:** Pillow, para la flecha suave: `py -3 -m pip install -U pillow` (con `-3`, el mismo Python que usa `iniciar_overlay.bat`).
- NMS en modo **Sin bordes / Borderless** (Opciones → Gráficos). En pantalla completa el recuadro queda detrás del juego.

## Configurar NMS.py (una vez por versión)

En PowerShell: `py -3.13 -m pymhf config nmspy`

1. **Set mod directory** → elige la carpeta `GAMEDATA\MODS` **de esta versión** (por ejemplo `...\OlympianPlanetCoords_v0.0.3\GAMEDATA\MODS`). Cada versión nueva vive en su propia carpeta, así que este paso se repite al actualizar.
2. **Recomendado:** **Set game to start paused** → **No**. Si queda en Sí y NMS.py abre el juego por su cuenta, el juego se queda congelado al arrancar.

## Cada vez que juegues

1. Abre **No Man's Sky desde Steam** y **quédate en el menú principal**.
2. En PowerShell: `py -3.13 -m pymhf run nmspy`. Debe decir `Found an already running instance` y en la ventana del registro `Loaded … mods`.
3. Doble clic en `OLYMPIAN_OVERLAY\iniciar_overlay.bat`.
4. Carga tu partida.

## Uso

- Formatos: `+12.34, -56.78` · `12.34 -56.78 Mi base` · `12,34; -56,78; Mi base` (coma decimal → `;`).
- **Esc** cancela. **Clic derecho**: Tamaño, Tecla, Quitar destino de este planeta, Acerca de, Mover, Salir.
- El rumbo es respecto al norte del planeta. La flecha es respecto al frente de tu nave (o de tu personaje a pie).

## Ajustes en la ventana de NMS.py (se guardan solos)

| Ajuste | Para qué |
|---|---|
| Cal. longitud / Cal. inv. latitud / Cal. centro local | Si las coordenadas no coinciden con el Visor de análisis. |
| Posicion / Pos. + offset | Dejar en `Entorno` / desmarcado (sigue a la nave). |
| Flecha inv. | Si la flecha apunta al revés (hacia atrás). |
| Marcador | Dejar en `BaseBuildingHUD`. |
| Tecla (Reload Mod) | La misma tecla del menú del recuadro. Se activa con **Reload Mod**. |

## Si algo falla, incluye esto al reportarlo (Issues de GitHub)

1. Las últimas ~40 líneas de la ventana del registro de NMS.py.
2. `http://localhost:5000/OlympianPlanetCoords/debug` (en el navegador, estando en un planeta).
3. Si la flecha no se ve suave: la ventana del overlay se abre con `pyw`, que no muestra mensajes. Ábrelo con `py -3 OLYMPIAN_OVERLAY\olympian_overlay.py` en PowerShell y copia lo que diga.

## Lo que hace y lo que no hace Olympian Planet Coords

Todo mod o editor cambia el juego, y Olympian Planet Coords no es la excepción. Lo que lo distingue no es una superioridad moral, sino estos compromisos de seguridad:

1. **Solo presentará resultados que el juego podría producir por sí mismo**, siguiendo sus propias reglas y límites. Nunca dejará tu partida en un estado imposible.
2. **Solo modificará aquello que se sabe con certeza; no destruirá tus saves.** No aplicará funciones experimentales de alto riesgo: ya el riesgo lo corremos por ti, con pruebas reales continuamente monitoreadas. De todo lo demás podremos mostrarte una lectura, pero hasta no definir un método seguro, no ofreceremos edición.
3. **Tratará de mantener tu experiencia del juego lo más fiel posible** (sin dejar de ofrecer mejoras opcionales). Verás que ofrece herramientas para resolver problemas reales, no atajos para "tenerlo todo".
4. **Pondrá tu seguridad primero:** respaldos antes de cualquier cambio a tu partida, lo mínimo invasivo posible y avisos claros cuando algo tenga riesgo.
5. **Será transparente:** código abierto, sin relación con Hello Games, y explicará qué hace y qué no hace cada función.

En este mod: solo lee la posición del jugador y pone un marcador del HUD como los que el juego ya usa. No modifica la partida guardada, el inventario ni nada que se guarde en el save.

## Divulgación sobre el uso de IA

El uso de nuevas tecnologías forma parte del día a día de este y de otros proyectos de MightyZeus. Olympian Planet Coords se desarrolla con la ayuda de una IA (Claude), que participa en la escritura y revisión del código; la dirección del proyecto, qué funciones entran y cuáles se dejan fuera a propósito, y las pruebas en el juego real son trabajo humano.
El compromiso de fondo no cambia: crear soluciones y herramientas reales desde la perspectiva de un jugador apasionado por disfrutar del juego y por ayudar a otros a hacerlo.

## Licencias de terceros

- Olympian Planet Coords: GNU GPL v3.0 (archivo LICENSE).
- NMS.py / pyMHF — monkeyman192, licencia MIT.
- Pillow — licencia MIT-CMU (se instala aparte con pip).
- Fuentes DSEG7 Classic (keshikan) y Share Tech Mono (Carrois Type Design) — SIL Open Font License 1.1 (textos en `OLYMPIAN_OVERLAY\fonts\`).

## Otros proyectos de MightyZeus para No Man's Sky

- **Olympian NMS Save Editor**: editor de partidas seguro y visual (PC/Steam). https://github.com/ImMightyZeus/Olympian-NMS-Save-Editor

Todos los proyectos: https://github.com/ImMightyZeus
