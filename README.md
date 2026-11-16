# Olympian Planet Coords

**[Español](#español) · [English](#english)**

---

## Español

```
Olympian Planet Coords — Copyright © 2026 MightyZeus · Licencia por definir
Apoya el proyecto: https://paypal.me/ImMightyZeus · Repositorio: https://github.com/ImMightyZeus/Olympian-Planet-Coords

Agradecimientos:
A mi querida esposa y mi amada familia, por permitirme seguir siendo un niño aun en mi adultez.
Pangro Daer Barun D'Gross, por seguirme corrompiendo con videojuegos.
Fenrir, porque aunque ya casi no juega, aún se presta a conversar sobre mis aventuras y desventuras.
```

Mod para **No Man's Sky** (PC, Steam, Windows) que muestra las **coordenadas del planeta en tiempo real** y te deja fijar **destinos** con un marcador en el HUD y una **flecha 3D** que apunta hacia ellos.

### Qué incluye (v0.0.3)

- Coordenadas en vivo arriba a la izquierda: a pie, en nave, en vehículo y en órbita.
- Clic en el recuadro (o **F9** en el juego) → escribe `lat, lon [nombre]` → **Enter** = destino con marcador en el HUD.
- Distancia y rumbo al destino, y una flecha 3D con bordes suaves que gira e inclina hacia él.
- Al llegar (50 m o menos) el destino se quita solo y la flecha se convierte en una esfera.
- Un destino por planeta, recordado aunque te vayas; historial de los últimos 10.
- Tamaño Normal / Grande y tecla configurable.

### Estado

Esta es la **versión de laboratorio** en Python: funciona con [NMS.py](https://github.com/monkeyman192/NMS.py) y sirve para probar cada cambio en el juego real antes de llevarlo a la versión para jugadores. La versión para jugadores será un **DLL** que solo se copia junto a `NMS.exe` (carpeta `dll/`, en desarrollo).

Instalación y uso: [INSTRUCCIONES.md](INSTRUCCIONES.md).

### Lo que hace y lo que no hace Olympian Planet Coords

Todo mod o editor cambia el juego, y Olympian Planet Coords no es la excepción. Lo que lo distingue no es una superioridad moral, sino estos compromisos de seguridad:

1. **Solo presentará resultados que el juego podría producir por sí mismo**, siguiendo sus propias reglas y límites. Nunca dejará tu partida en un estado imposible.
2. **Solo modificará aquello que se sabe con certeza; no destruirá tus saves.** No aplicará funciones experimentales de alto riesgo: ya el riesgo lo corremos por ti, con pruebas reales continuamente monitoreadas. De todo lo demás podremos mostrarte una lectura, pero hasta no definir un método seguro, no ofreceremos edición.
3. **Tratará de mantener tu experiencia del juego lo más fiel posible** (sin dejar de ofrecer mejoras opcionales). Verás que ofrece herramientas para resolver problemas reales, no atajos para "tenerlo todo".
4. **Pondrá tu seguridad primero:** respaldos antes de cualquier cambio a tu partida, lo mínimo invasivo posible y avisos claros cuando algo tenga riesgo.
5. **Será transparente:** código abierto, sin relación con Hello Games, y explicará qué hace y qué no hace cada función.

En este mod: solo lee la posición del jugador y pone un marcador del HUD como los que el juego ya usa. No modifica la partida guardada, el inventario ni nada que se guarde en el save.

### Aviso

Proyecto hecho por fans, sin relación con Hello Games. No Man's Sky es una marca de Hello Games. Úsalo bajo tu propia responsabilidad.

### Divulgación sobre el uso de IA

El uso de nuevas tecnologías forma parte del día a día de este y de otros proyectos de MightyZeus. Olympian Planet Coords se desarrolla con la ayuda de una IA (Claude), que participa en la escritura y revisión del código; la dirección del proyecto, qué funciones entran y cuáles se dejan fuera a propósito, y las pruebas en el juego real son trabajo humano.
El compromiso de fondo no cambia: crear soluciones y herramientas reales desde la perspectiva de un jugador apasionado por disfrutar del juego y por ayudar a otros a hacerlo.

### Licencia

Por definir antes de la publicación. Componentes de terceros:

- NMS.py / pyMHF — monkeyman192, MIT (los patrones de `dll/tools/nmspy_data_185034.json` vienen de NMS.py; ver `dll/tools/NMS.py-LICENSE.txt`).
- MinHook — Tsuda Kageyu, BSD-2-Clause (`dll/third_party/MinHook-LICENSE.txt`).
- Pillow — MIT-CMU (se instala aparte).
- Fuentes DSEG7 Classic y Share Tech Mono — SIL Open Font License 1.1 (`overlay/fonts/`).

### Otros proyectos de MightyZeus para No Man's Sky

- **Olympian NMS Save Editor**: editor de partidas seguro y visual (PC/Steam). https://github.com/ImMightyZeus/Olympian-NMS-Save-Editor

Todos los proyectos: https://github.com/ImMightyZeus

---

## English

```
Olympian Planet Coords — Copyright © 2026 MightyZeus · License to be defined
Support the project: https://paypal.me/ImMightyZeus · Repository: https://github.com/ImMightyZeus/Olympian-Planet-Coords

Acknowledgements:
To my dear wife and my beloved family, for letting me keep being a kid even as an adult.
Pangro Daer Barun D'Gross, for continuing to corrupt me with video games.
Fenrir, because even though he hardly plays anymore, he's still up for talking about my adventures and misadventures.
```

A **No Man's Sky** mod (PC, Steam, Windows) that shows **live planet coordinates** and lets you set **destinations** with a HUD marker and a **3D arrow** pointing at them.

### What's included (v0.0.3)

- Live coordinates in the top-left corner: on foot, in a ship, in a vehicle and in orbit.
- Click the box (or press **F9** in game) → type `lat, lon [name]` → **Enter** = destination with a HUD marker.
- Distance and bearing to the destination, and a smooth-edged 3D arrow that turns and tilts towards it.
- On arrival (50 m or less) the destination clears itself and the arrow turns into a sphere.
- One destination per planet, remembered even if you leave; history of the last 10.
- Normal / Large size and a configurable hotkey.

### Status

This is the Python **lab version**: it runs on [NMS.py](https://github.com/monkeyman192/NMS.py) and is used to test every change in the real game before it reaches the player version. The player version will be a **DLL** you simply copy next to `NMS.exe` (`dll/` folder, in development).

Installation and usage: [INSTRUCTIONS.md](INSTRUCTIONS.md).

### What Olympian Planet Coords does and doesn't do

Every mod or editor changes the game, and Olympian Planet Coords is no exception. What sets it apart isn't moral superiority, but these safety commitments:

1. **It will only produce results the game could produce on its own**, following its own rules and limits. It will never leave your save in an impossible state.
2. **It will only change what is known with certainty; it will not destroy your saves.** It will not apply high-risk experimental features: we already take that risk for you, with real tests that are continuously monitored. For everything else we may show you a reading, but until a safe method is defined, we will not offer editing.
3. **It will try to keep your game experience as faithful as possible** (while still offering optional improvements). You'll see that it offers tools that solve real problems, not shortcuts to "have everything".
4. **It will put your safety first:** backups before any change to your save, as little intrusion as possible, and clear warnings when something carries risk.
5. **It will be transparent:** open source, not affiliated with Hello Games, and it will explain what each feature does and doesn't do.

In this mod: it only reads the player's position and places a HUD marker like the ones the game already uses. It does not modify your save, your inventory or anything stored in the save file.

### Disclaimer

Fan-made project, not affiliated with Hello Games. No Man's Sky is a trademark of Hello Games. Use at your own risk.

### AI Disclosure

New technologies are part of the everyday work behind this and other MightyZeus projects. Olympian Planet Coords is developed with the help of an AI (Claude), which takes part in writing and reviewing the code; the project's direction, which features go in and which are left out on purpose, and the testing in the real game are human work.
The underlying commitment doesn't change: building real solutions and tools from the perspective of a player who is passionate about enjoying the game and helping others do the same.

### License

To be defined before release. Third-party components:

- NMS.py / pyMHF — monkeyman192, MIT (the patterns in `dll/tools/nmspy_data_185034.json` come from NMS.py; see `dll/tools/NMS.py-LICENSE.txt`).
- MinHook — Tsuda Kageyu, BSD-2-Clause (`dll/third_party/MinHook-LICENSE.txt`).
- Pillow — MIT-CMU (installed separately).
- DSEG7 Classic and Share Tech Mono fonts — SIL Open Font License 1.1 (`overlay/fonts/`).

### Other MightyZeus projects for No Man's Sky

- **Olympian NMS Save Editor**: safe, visual save editor (PC/Steam). https://github.com/ImMightyZeus/Olympian-NMS-Save-Editor

All projects: https://github.com/ImMightyZeus

---

## Desarrollo / Development

```
mod/        mod de NMS.py / NMS.py mod
overlay/    recuadro y flecha (tkinter + Pillow) / box and arrow
tests/      pruebas sin el juego / tests without the game
dll/        versión DLL en desarrollo / DLL version in development (mingw-w64 + MinHook)
```

Pruebas / Tests (Python 3.12+, Linux o Windows):

```
pip install nmspy==185034.0 pillow fastapi uvicorn httpx
cd tests && python test_mod.py && python test_overlay.py
```

DLL: `cd dll && sh build.sh` (requiere / requires `x86_64-w64-mingw32-g++`).
