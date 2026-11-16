# Olympian Planet Coords — v0.0.3

```
Olympian Planet Coords — Copyright © 2026 MightyZeus · License to be defined (before release)
Support the project: https://paypal.me/ImMightyZeus · Repository: https://github.com/ImMightyZeus/Olympian-Planet-Coords

Acknowledgements:
To my dear wife and my beloved family, for letting me keep being a kid even as an adult.
Pangro Daer Barun D'Gross, for continuing to corrupt me with video games.
Fenrir, because even though he hardly plays anymore, he's still up for talking about my adventures and misadventures.
```

Developed by **MightyZeus**, from Nicaragua. *Español: [INSTRUCCIONES.md](INSTRUCCIONES.md).*

> These instructions are for the **release package** of each version (folder `OlympianPlanetCoords_vX.Y.Z`, with `GAMEDATA\MODS` and `OLYMPIAN_OVERLAY`). In the repository the same files live in `mod/` and `overlay/`.
>
> The mod's on-screen texts are currently in Spanish only; an English interface is planned.

---

A No Man's Sky mod (PC, Steam, Windows):

1. **Live coordinates** in the top-left corner (on foot, in a ship, in a vehicle and in orbit).
2. **Click** the box (or **F9** in game) → type `lat, lon [name]` → **Enter** = destination with a HUD marker.
3. **Distance and bearing** to the destination, and a smooth-edged **3D arrow** that turns and tilts towards it.
4. On arrival (50 m or less) the destination clears itself; the arrow turns into a sphere and shrinks.
5. **One destination per planet**, remembered even if you leave; it only shows on that planet.
6. **History** of the last 10 destinations (↑ ↓ arrows in the text field).
7. **Normal / Large size** and a configurable **hotkey** (right-click the box).

## What's new in v0.0.3

- **F9 is the default hotkey** (previously F8). F8 is used by Imperial Galactic Map for "add to favourites". Change it with right-click → **Tecla** (F5 to F11, or Ninguna = none); it takes effect after **Reload Mod** in the NMS.py window.
- **No longer stuck on "esperando al juego"** ("waiting for the game") if NMS.py is started with a save already loaded.
- **Smooth-edged arrow**: drawn with Pillow and shown with real per-pixel transparency. Without Pillow, the previous arrow is used.
- "Acerca de" (About) window with the repository, the AI disclosure and other projects.

## Folder contents

```
GAMEDATA\MODS\OlympianPlanetCoords.py   ← the mod (NMS.py loads it from here)
OLYMPIAN_OVERLAY\                        ← the box and the arrow
INSTRUCCIONES.md / INSTRUCTIONS.md
```

**Don't copy anything into the game folder.** NMS.py loads the mod from this folder. If the game's own `GAMEDATA\MODS` has any files (even a `__pycache__`), NMS shows the "Mod Warning" at startup: that game folder must be empty or not exist.

## Requirements (once)

- Python **3.10 to 3.13** from python.org (3.14 doesn't work with NMS.py yet). To see which you have: `py --list`.
- NMS.py: `py -3.13 -m pip install -U nmspy` (replace 3.13 with your version).
- **New:** Pillow, for the smooth arrow: `py -3 -m pip install -U pillow` (with `-3`, the same Python that `iniciar_overlay.bat` uses).
- NMS in **Borderless** mode (Options → Graphics). In fullscreen the box stays behind the game.

## Configure NMS.py (once per version)

In PowerShell: `py -3.13 -m pymhf config nmspy`

1. **Set mod directory** → choose the `GAMEDATA\MODS` folder **of this version** (for example `...\OlympianPlanetCoords_v0.0.3\GAMEDATA\MODS`). Each new version lives in its own folder, so repeat this step when you update.
2. **Recommended:** **Set game to start paused** → **No**. If left on Yes and NMS.py launches the game itself, the game freezes on startup.

## Every time you play

1. Launch **No Man's Sky from Steam** and **stay on the main menu**.
2. In PowerShell: `py -3.13 -m pymhf run nmspy`. It should say `Found an already running instance`, and the log window `Loaded … mods`.
3. Double-click `OLYMPIAN_OVERLAY\iniciar_overlay.bat`.
4. Load your save.

## Usage

- Formats: `+12.34, -56.78` · `12.34 -56.78 My base` · `12,34; -56,78; My base` (decimal comma → `;`).
- **Esc** cancels. **Right-click**: Tamaño (size), Tecla (hotkey), Quitar destino de este planeta (clear this planet's destination), Acerca de (about), Mover (move), Salir (exit).
- The bearing is relative to the planet's north. The arrow is relative to the front of your ship (or your character on foot).

## Settings in the NMS.py window (saved automatically)

| Setting | What it's for |
|---|---|
| Cal. longitud / Cal. inv. latitud / Cal. centro local | Longitude / latitude / local-centre calibration, if the coordinates don't match the Analysis Visor. |
| Posicion / Pos. + offset | Position source: leave on `Entorno` / unchecked (follows the ship). |
| Flecha inv. | Invert arrow, if it points backwards. |
| Marcador | Marker method: leave on `BaseBuildingHUD`. |
| Tecla (Reload Mod) | Same hotkey as the box menu. Takes effect after **Reload Mod**. |

## If something fails, include this when reporting it (GitHub Issues)

1. The last ~40 lines of the NMS.py log window.
2. `http://localhost:5000/OlympianPlanetCoords/debug` (in a browser, while on a planet).
3. If the arrow doesn't look smooth: the overlay starts with `pyw`, which shows no messages. Run `py -3 OLYMPIAN_OVERLAY\olympian_overlay.py` in PowerShell and copy what it prints.

## What Olympian Planet Coords does and doesn't do

Every mod or editor changes the game, and Olympian Planet Coords is no exception. What sets it apart isn't moral superiority, but these safety commitments:

1. **It will only produce results the game could produce on its own**, following its own rules and limits. It will never leave your save in an impossible state.
2. **It will only change what is known with certainty; it will not destroy your saves.** It will not apply high-risk experimental features: we already take that risk for you, with real tests that are continuously monitored. For everything else we may show you a reading, but until a safe method is defined, we will not offer editing.
3. **It will try to keep your game experience as faithful as possible** (while still offering optional improvements). You'll see that it offers tools that solve real problems, not shortcuts to "have everything".
4. **It will put your safety first:** backups before any change to your save, as little intrusion as possible, and clear warnings when something carries risk.
5. **It will be transparent:** open source, not affiliated with Hello Games, and it will explain what each feature does and doesn't do.

In this mod: it only reads the player's position and places a HUD marker like the ones the game already uses. It does not modify your save, your inventory or anything stored in the save file.

## AI Disclosure

New technologies are part of the everyday work behind this and other MightyZeus projects. Olympian Planet Coords is developed with the help of an AI (Claude), which takes part in writing and reviewing the code; the project's direction, which features go in and which are left out on purpose, and the testing in the real game are human work.
The underlying commitment doesn't change: building real solutions and tools from the perspective of a player who is passionate about enjoying the game and helping others do the same.

## Third-party licenses

- NMS.py / pyMHF — monkeyman192, MIT license.
- Pillow — MIT-CMU license (installed separately with pip).
- DSEG7 Classic (keshikan) and Share Tech Mono (Carrois Type Design) fonts — SIL Open Font License 1.1 (texts in `OLYMPIAN_OVERLAY\fonts\`).

## Other MightyZeus projects for No Man's Sky

- **Olympian NMS Save Editor**: safe, visual save editor (PC/Steam). https://github.com/ImMightyZeus/Olympian-NMS-Save-Editor

All projects: https://github.com/ImMightyZeus
