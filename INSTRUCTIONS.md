# Olympian Planet Coords — v0.2.0-beta

```
Olympian Planet Coords — Copyright © 2026 MightyZeus · GPL-3.0 License
Support the project: https://paypal.me/ImMightyZeus · Repository: https://github.com/ImMightyZeus/Olympian-Planet-Coords

Acknowledgements:
To my dear wife and my beloved family, for letting me keep being a kid even as an adult.
Pangro Daer Barun D'Gross, for continuing to corrupt me with video games.
Fenrir, because even though he hardly plays anymore, he's still up for talking about my adventures and misadventures.
```

Developed by **MightyZeus**, from Nicaragua. *Español: [INSTRUCCIONES.md](INSTRUCCIONES.md).*

> **Beta version.** A DLL you copy next to the game. From this version on, everything is shown and controlled **inside the game**: the `.exe` box is optional. No Python or NMS.py needed. If you find a rough edge, please report it (see the end).

---

Mod for No Man's Sky (PC, Steam, Windows):

1. **Live coordinates** in a box inside the game, top-left (on foot, in a ship, in a vehicle and in orbit).
2. **F9** opens the in-game **panel**: type `lat, lon [name]` → **Enter** = destination with a marker on the game's HUD. While the panel is open, the game gets no keyboard, mouse or gamepad input.
3. **Destination coordinates, distance and bearing**, and a **3D arrow** pointing at it.
4. On arrival (50 m or less) the destination clears itself; the arrow turns into a sphere.
5. **One destination per planet**, remembered even if you leave; it only shows on that planet.
6. **Saved destinations for each planet** (up to 10): in the panel, ↑ ↓ to pick, **Enter** to go, **Del** to remove.
7. **Away from planets:** in a station, its name and below it the system and galaxy; flying in space, the system and below it the galaxy.
8. **Normal / Large** size, configurable **hotkey** and **language** English / Spanish / Russian (in the panel: **Tab** → Settings).

## What's new in v0.2.0-beta

- **Everything inside the game:** box and panel drawn inside the game (DSEG7 and Share Tech Mono fonts, like the `.exe`). No more borderless mode or `.exe` needed.
- **Keyboard panel** (F9): destination, this planet's destination list, settings.
- **Per-planet list:** saved destinations only show on their planet.
- **Station, system and galaxy names** when you're not on a planet.
- **Arrow fixed:** with far destinations it used to point at you; now it points along the ground, towards the destination.
- **For the game update** NMS.exe 186923.
- **Russian** language.

## Package contents

```
Binaries\winmm.dll                 ← the mod (goes in the game's Binaries folder)
OlympianPlanetCoords-Overlay.exe   ← optional: the box outside the game, as in v0.1.0
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
3. Done. The overlay `.exe` is **optional** (for anyone who prefers the box outside the game, in borderless mode). The first time you open it, Windows may show "Windows protected your PC" (SmartScreen) because it isn't digitally signed yet: **More info** → **Run anyway**. You can compare its hash with `SHA256SUMS.txt` (in PowerShell: `Get-FileHash OlympianPlanetCoords-Overlay.exe`).

## Every time you play

1. Launch **No Man's Sky from Steam**, as usual.
2. Load your save. Within a few seconds the box with your coordinates appears, top-left.

## Usage

- **F9** opens and closes the panel. **Esc** closes it. **Tab** switches between **Destination** and **Settings**.
- **Destination:** type and press **Enter**. Formats: `+12.34, -56.78` · `12.34 -56.78 My base` · `12,34; -56,78; My base` (decimal comma → `;`). **↑ ↓** picks a saved destination on this planet: **Enter** goes there, **Del** removes it from the list.
- **Settings:** **↑ ↓** to choose and **← →** to change: language, panel key (F5 to F11), box size (Normal / Large) and "Clear this planet's destination" (**Enter**).
- While the panel is open, the game gets no keyboard, mouse or gamepad input: your character and ship don't move while you type. If you switch windows (Alt+Tab), the panel closes.
- **On laptops:** if pressing F9 opens something else (the browser, brightness, volume…), that is the key's special function on your keyboard. Use **Fn + F9**, turn on your laptop's Fn lock, or pick another key in Settings.
- The bearing is relative to the planet's north. The arrow is relative to where you're facing (your character on foot or your ship's front): straight up means the destination is ahead.
- If you ever want to turn off the in-game box (and go back to the `.exe`): in `%LOCALAPPDATA%\OlympianPlanetCoords\datos.json`, inside `"settings"`, set `"ingame_box": false`.

## Game version

This beta works with **No Man's Sky build 186923** (the version of the `NMS.exe` file). When the game updates, the DLL detects it and **disables itself**: the game runs normally, without the box. In that case, wait for the mod version for that patch.

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

In this mod: it only reads the player's position and the system and station names, places a HUD marker like the ones the game already uses (the game draws it, and it is requested again every frame) and draws its box over the game's image. It does not modify your save, your inventory or anything stored in the save file. It doesn't touch any game files: it only adds `winmm.dll`, which you delete to uninstall.

## AI Disclosure

New technologies are part of the everyday work behind this and other MightyZeus projects. Olympian Planet Coords is developed with the help of an AI (Claude), which takes part in writing and reviewing the code; the project's direction, which features go in and which are left out on purpose, and the testing in the real game are human work.
The underlying commitment doesn't change: building real solutions and tools from the perspective of a player who is passionate about enjoying the game and helping others do the same.

## Third-party licenses

Full texts in `THIRD_PARTY_LICENSES\`.

- Olympian Planet Coords: GNU GPL v3.0 (LICENSE file).
- MinHook — Tsuda Kageyu, BSD-2-Clause license (inside the DLL).
- JSON for Modern C++ (nlohmann/json) — Niels Lohmann, MIT license (inside the DLL).
- Game data positions taken from NMS.py / pyMHF — monkeyman192, MIT license (the function patterns are our own).
- Inside the `.exe` (optional): Python (PSF license), Pillow (MIT-CMU license) and the PyInstaller bootloader (GPL with exception).
- DSEG7 Classic (keshikan) and Share Tech Mono (Carrois Type Design) fonts — SIL Open Font License 1.1 (inside the DLL and the `.exe`).

## Other MightyZeus projects for No Man's Sky

- **Olympian NMS Save Editor**: safe, visual save editor (PC/Steam). https://github.com/ImMightyZeus/Olympian-NMS-Save-Editor

All projects: https://github.com/ImMightyZeus
