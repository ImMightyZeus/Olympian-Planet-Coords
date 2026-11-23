"""Genera dll/src/nms_offsets.h con los desplazamientos de las estructuras del juego que usa el DLL.

Los desplazamientos salen de las definiciones de NMS.py (nmspy, licencia MIT) para la misma version
del juego que los patrones de funciones. Son los mismos que usa la version Python del mod, que ya
se probo en el juego.

Uso (desde dll/, con nmspy instalado; en Linux importar antes tests/winstubs.py):
    python tools/gen_offsets.py

Olympian Planet Coords — Copyright © 2026 MightyZeus · GPL-3.0 License
"""

import ctypes
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "tests"))
if sys.platform != "win32":
    import winstubs  # noqa: F401

import nmspy  # noqa: E402
import nmspy.data.enums as en  # noqa: E402
import nmspy.data.types as nms  # noqa: E402


def off(cls, name):
    return getattr(cls, name).offset


def field_type(cls, name):
    return dict(cls._fields_)[name]


def main():
    A, D, S, E = nms.cGcApplication, nms.cGcApplication.Data, nms.cGcSimulation, nms.cGcEnvironment
    PE, GS, PS, PL = nms.cGcPlayerEnvironment, nms.cGcGameState, nms.cGcPlayerState, nms.cGcPlayer
    loc_t = field_type(PS, "mLocation")
    ga_t = field_type(loc_t, "GalacticAddress")
    tm_t = field_type(PE, "mPlayerTM")
    gm_t = field_type(PL, "mGraphicsMatrix")
    rel_t = field_type(gm_t, "pos")
    vals = {
        "APP_MPDATA": off(A, "mpData"),
        "DATA_SIMULATION": off(D, "mSimulation"),
        "DATA_GAMESTATE": off(D, "mGameState"),
        "SIM_ENVIRONMENT": off(S, "mEnvironment"),
        "SIM_PLAYER": off(S, "mPlayer"),
        "SIM_SOLAR_SYSTEM": off(S, "mpSolarSystem"),
        "ENV_PLAYERENV": off(E, "mPlayerEnvironment"),
        "PENV_PLAYERTM": off(PE, "mPlayerTM"),
        "PENV_NEAREST_PLANET_INDEX": off(PE, "miNearestPlanetIndex"),
        "PENV_DISTANCE_FROM_PLANET": off(PE, "mfDistanceFromPlanet"),
        "PENV_NEAREST_PLANET_SEALEVEL": off(PE, "mfNearestPlanetSealevel"),
        "PENV_NEAREST_PLANET_POS": off(PE, "mNearestPlanetPos"),
        "PENV_INSIDE_ATMOSPHERE": off(PE, "mbInsidePlanetAtmosphere"),
        "PENV_LOCATION_STABLE": off(PE, "meLocationStable"),
        "TM_RIGHT": off(tm_t, "right"),
        "TM_UP": off(tm_t, "up"),
        "TM_AT": off(tm_t, "at"),
        "TM_POS": off(tm_t, "pos"),
        "GAMESTATE_PLAYERSTATE": off(GS, "mPlayerState"),
        "PSTATE_LOCATION": off(PS, "mLocation"),
        "LOC_GALACTIC_ADDRESS": off(loc_t, "GalacticAddress"),
        "LOC_REALITY_INDEX": off(loc_t, "RealityIndex"),
        "GA_SOLAR_SYSTEM_INDEX": off(ga_t, "SolarSystemIndex"),
        "GA_VOXEL_X": off(ga_t, "VoxelX"),
        "GA_VOXEL_Y": off(ga_t, "VoxelY"),
        "GA_VOXEL_Z": off(ga_t, "VoxelZ"),
        "PLAYER_GRAPHICS_MATRIX": off(PL, "mGraphicsMatrix"),
        "GM_POS": off(gm_t, "pos"),
        "REL_LOCAL": off(rel_t, "local"),
        "REL_OFFSET": off(rel_t, "offset"),
    }
    locs = {m.name: m.value for m in en.EnvironmentLocation.Enum}
    version = getattr(nmspy, "__version__", "?")
    out = [
        "// Generado por tools/gen_offsets.py desde nmspy " + str(version) + " (MIT). No editar a mano.",
        "// Desplazamientos (bytes) de las estructuras del juego que lee Olympian Planet Coords.",
        "#pragma once",
        "",
        "namespace nmsoff {",
    ]
    for k, v in vals.items():
        out.append(f"constexpr unsigned long long {k} = 0x{v:x};  // {v}")
    out.append("")
    out.append("// EnvironmentLocation::Enum")
    for k, v in locs.items():
        out.append(f"constexpr int LOC_{k.rstrip('_').upper()} = {v};")
    out.append("}  // namespace nmsoff")
    dst = os.path.join(HERE, "..", "src", "nms_offsets.h")
    with open(dst, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(out) + "\n")
    print("OK:", os.path.normpath(dst))


if __name__ == "__main__":
    main()
