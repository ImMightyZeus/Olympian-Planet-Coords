# -*- mode: python ; coding: utf-8 -*-
# PyInstaller: overlay de Olympian Planet Coords como un solo .exe (sin Python para el jugador).
# Overlay of Olympian Planet Coords as a single .exe (no Python needed for the player).
#
# Uso / usage (desde la raiz del repositorio / from the repository root):
#   pyinstaller --noconfirm --clean build_tools/overlay.spec
#
# Olympian Planet Coords — Copyright © 2026 MightyZeus · GPL-3.0 License

import os

ROOT = os.path.abspath(os.path.join(SPECPATH, ".."))
OV = os.path.join(ROOT, "overlay")
IS_WIN = os.name == "nt"

datas = [
    (os.path.join(OV, "fonts"), "fonts"),
    (os.path.join(OV, "olympian_planet_coords.ico"), "."),
    (os.path.join(OV, "olympian_planet_coords.png"), "."),
]

a = Analysis(
    [os.path.join(OV, "olympian_overlay.py")],
    pathex=[OV],
    datas=datas,
    hiddenimports=["PIL.ImageTk"],
    excludes=["numpy", "matplotlib", "pytest", "unittest", "pydoc_data"],
    noarchive=False,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.datas,
    [],
    name="OlympianPlanetCoords-Overlay",
    console=False,          # sin ventana negra / no console window
    upx=False,              # UPX aumenta los falsos positivos de antivirus / UPX raises AV false positives
    icon=os.path.join(OV, "olympian_planet_coords.ico") if IS_WIN else None,
    version=os.path.join(SPECPATH, "overlay_version.txt") if IS_WIN else None,
)
