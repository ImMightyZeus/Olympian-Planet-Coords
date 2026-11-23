# Olympian Planet Coords — v0.1.0-beta

```
Olympian Planet Coords — Copyright © 2026 MightyZeus · GPL-3.0 License
Support the project: https://paypal.me/ImMightyZeus · Repository: https://github.com/ImMightyZeus/Olympian-Planet-Coords

Acknowledgements:
To my dear wife and my beloved family, for letting me keep being a kid even as an adult.
Pangro Daer Barun D'Gross, for continuing to corrupt me with video games.
Fenrir, because even though he hardly plays anymore, he's still up for talking about my adventures and misadventures.
```

Developed by **MightyZeus**, from Nicaragua. *Español: [INSTRUCCIONES.md](INSTRUCCIONES.md).*

> **Beta version.** This is the first version for players: a DLL you copy next to the game and an on-screen box (`.exe`). No Python or NMS.py needed. It has been tested for hours in the real game, but it may still have rough edges: if you find one, please report it (see the end).

---

Mod for No Man's Sky (PC, Steam, Windows):

1. **Live coordinates** in the top-left corner (on foot, in a ship, in a vehicle and in orbit).
2. **Click** the box (or press **F9** in game) → type `lat, lon [name]` → **Enter** = destination with a marker on the game's HUD. Focus returns to the game on its own.
3. **Distance and bearing** to the destination, plus a smooth-edged **3D arrow** that turns and tilts towards it.
4. On arrival (50 m or less) the destination clears itself; the arrow turns into a sphere and shrinks.
5. **One destination per planet**, remembered even if you leave; it only shows up on that planet.
6. **History** of the last 10 destinations (↑ ↓ arrows in the text field).
7. **Size** Normal / Large, configurable **hotkey** and **language** English / Spanish / Russian (right-click the box).

## What's new in v0.1.0-beta

- **DLL version:** no more Python, NMS.py or commands. You copy one file next to the game and launch the game from Steam.
- **The box is now a program** (`OlympianPlanetCoords-Overlay.exe`), with Pillow included.
- **Hotkey changes take effect immediately** (right-click → **Key**): no more "Reload Mod".
- Destinations, history and settings from the Python version are kept: it's the same `datos.json` file.

## Package contents

```
Binaries\winmm.dll                 ← the mod (goes in the game's Binaries folder)
OlympianPlanetCoords-Overlay.exe   ← the box, the arrow and the menu
INSTRUCCIONES.md / INSTRUCTIONS.md
LICENSE                            ← GNU GPL v3
THIRD_PARTY_LICENSES\              ← licenses of third-party components
SHA256SUMS.txt                     ← hashes to check the files
```

## Installation (once)

1. In Steam: right-click **No Man's Sky** → **Manage** → **Browse local files**. The game folder opens.
2. Copy `Binaries\winmm.dll` from the package into the game's **`Binaries`** folder (the one with `NMS.exe`).
   - If Windows says a `winmm.dll` already exists there, **don't replace it**: it belongs to another mod. Let us know.
   - It works alongside Imperial Galactic Map (IGM uses a different file, `xinput9_1_0.dll`).
3. Copy `OlympianPlanetCoords-Overlay.exe` wherever you like (for example, the desktop).
4. In game: Options → Graphics → **Borderless**. In exclusive fullscreen the box stays behind the game.

**The first time you open the `.exe`**, Windows may show "Windows protected your PC" (SmartScreen), because the program isn't digitally signed yet. Click **More info** → **Run anyway**. To check that the file is the original, compare its hash with `SHA256SUMS.txt` (in PowerShell: `Get-FileHash OlympianPlanetCoords-Overlay.exe`).

## Every time you play

1. Launch **No Man's Sky from Steam**, as usual.
2. Open `OlympianPlanetCoords-Overlay.exe` (before or after the game, either way).
3. Load your save. Within a few seconds the box shows the coordinates.

## Usage

- Formats: `+12.34, -56.78` · `12.34 -56.78 My base` · `12,34; -56,78; My base` (decimal comma → `;`).
- **Esc** cancels. **Right-click**: Size, Key, Idioma / Language / Язык, Clear this planet's destination, About, Move, Exit (shown in the chosen language).
- **Hotkey:** F9 by default (F8 is used by Imperial Galactic Map). You can pick F5 to F11, or "None (click only)". It only responds while the game is in the foreground.
- **On laptops:** if pressing F9 opens something else (the browser, brightness, volume…), that is the key's special function on your keyboard. Use **Fn + F9**, turn on your laptop's Fn lock, or pick another key in the menu.
- The bearing is relative to the planet's north. The arrow is relative to the front of your ship (or your character on foot).

## Game version

This beta works with **No Man's Sky build 185034** (the version of the `NMS.exe` file). When the game updates, the DLL detects it and **disables itself**: the game runs normally and the box stays on "Waiting for the mod". In that case, wait for the mod version for that patch.

## Uninstall

Delete `winmm.dll` from the game's `Binaries` folder (and the `Binaries\Olympian` folder, which holds the log). Your destinations live in `%LOCALAPPDATA%\OlympianPlanetCoords\`; delete it too if you're not coming back.

**Don't run the DLL version and the NMS.py lab version at the same time**: both use port 5000.

## If something fails, include this when reporting it (GitHub Issues)

1. The mod's log: `Binaries\Olympian\olympian.log` in the game folder (if Windows doesn't allow writing there: `%LOCALAPPDATA%\OlympianPlanetCoords\olympian.log`).
2. `http://localhost:5000/OlympianPlanetCoords/debug` (in a browser, with the game running and while on a planet).
3. What you were doing and, if you can, a screenshot.

## What Olympian Planet Coords does and doesn't do

Every mod or editor changes the game, and Olympian Planet Coords is no exception. What sets it apart isn't moral superiority, but these safety commitments:

1. **It will only produce results the game could produce on its own**, following its own rules and limits. It will never leave your save in an impossible state.
2. **It will only change what is known with certainty; it will not destroy your saves.** It will not apply high-risk experimental features: we already take that risk for you, with real tests that are continuously monitored. For everything else we may show you a reading, but until a safe method is defined, we will not offer editing.
3. **It will try to keep your game experience as faithful as possible** (while still offering optional improvements). You'll see that it offers tools that solve real problems, not shortcuts to "have everything".
4. **It will put your safety first:** backups before any change to your save, as little intrusion as possible, and clear warnings when something carries risk.
5. **It will be transparent:** open source, not affiliated with Hello Games, and it will explain what each feature does and doesn't do.

In this mod: it only reads the player's position and places a HUD marker like the ones the game already uses (the game draws it, and it is requested again every frame). It does not modify your save, your inventory or anything stored in the save file. It doesn't touch any game files: it only adds `winmm.dll`, which you delete to uninstall.

## AI Disclosure

New technologies are part of the everyday work behind this and other MightyZeus projects. Olympian Planet Coords is developed with the help of an AI (Claude), which takes part in writing and reviewing the code; the project's direction, which features go in and which are left out on purpose, and the testing in the real game are human work.
The underlying commitment doesn't change: building real solutions and tools from the perspective of a player who is passionate about enjoying the game and helping others do the same.

## Third-party licenses

Full texts in `THIRD_PARTY_LICENSES\`.

- Olympian Planet Coords: GNU GPL v3.0 (LICENSE file).
- MinHook — Tsuda Kageyu, BSD-2-Clause license (inside the DLL).
- JSON for Modern C++ (nlohmann/json) — Niels Lohmann, MIT license (inside the DLL).
- Game function patterns taken from NMS.py / pyMHF — monkeyman192, MIT license.
- Inside the `.exe`: Python (PSF license), Pillow (MIT-CMU license) and the PyInstaller bootloader (GPL with exception).
- DSEG7 Classic (keshikan) and Share Tech Mono (Carrois Type Design) fonts — SIL Open Font License 1.1.

## Other MightyZeus projects for No Man's Sky

- **Olympian NMS Save Editor**: safe, visual save editor (PC/Steam). https://github.com/ImMightyZeus/Olympian-NMS-Save-Editor

All projects: https://github.com/ImMightyZeus
