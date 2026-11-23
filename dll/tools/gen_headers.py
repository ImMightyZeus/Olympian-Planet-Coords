"""Genera los headers C++ del DLL a partir de datos publicos:

* nms_sigs.h      <- tools/sigs_<build>.json: patrones de bytes propios de las funciones del juego
                     que usa Olympian Planet Coords (sacados del NMS.exe con analisis estatico).
* winmm_stubs.S   <- tools/winmm_exports.txt: un "salto" por cada funcion de winmm.dll, para
                     que el DLL se haga pasar por winmm.dll y reenvie todo al de Windows.
* winmm.def       <- exportaciones del DLL.

Tras un parche del juego: generar tools/sigs_<build>.json con los patrones del NMS.exe nuevo (cada uno
debe aparecer una sola vez en .text), volver a ejecutar este script con --build <build> y recompilar.

Uso: python tools/gen_headers.py --build 185034
"""

import argparse
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "src")

FUNCTIONS = [
    "cGcApplication::Update",
    "cGcPlayerHUD::Update",
    "cGcPlayerEnvironment::IsOnPlanet",
    "cGcMarkerPoint::cGcMarkerPoint",
    "cGcMarkerList::TryAddMarker",
    "cGcMarkerList::RemoveMarker",
    "cGcBaseBuildingManager::Update",
    "cGcBaseBuildingManager::AddHUDMarker",
    "cTkFSM::StateChange",  # su "this" es cGcApplication (como hace NMS.py); Update no recibe el puntero
]


def c_ident(name: str) -> str:
    return name.replace("::", "_")


def gen_sigs(build: str) -> str:
    data = json.load(open(os.path.join(HERE, f"sigs_{build}.json"), encoding="utf-8"))
    by_name = {f["name"]: f for f in data["functions"]}
    missing = [n for n in FUNCTIONS if n not in by_name]
    if missing:
        raise SystemExit(f"Faltan en sigs_{build}.json: {missing}")
    lines = [
        "// GENERADO por tools/gen_headers.py - no editar a mano.",
        f"// Fuente: tools/sigs_{build}.json (patrones propios), build {build}.",
        "#pragma once",
        f'#define NMS_EXPECTED_BUILD "{build}"',
        "struct NmsSig { const char* name; const char* pattern; };",
        "enum NmsFn {",
    ]
    lines += [f"    FN_{c_ident(n)}," for n in FUNCTIONS]
    lines += ["    FN_COUNT", "};", "static const NmsSig kNmsSigs[FN_COUNT] = {"]
    lines += [f'    {{"{n}", "{by_name[n]["signature"].strip()}"}},' for n in FUNCTIONS]
    lines += ["};", ""]
    return "\n".join(lines)


def gen_winmm() -> tuple[str, str]:
    names = [n.strip() for n in open(os.path.join(HERE, "winmm_exports.txt")) if n.strip()]
    asm = [
        "// GENERADO por tools/gen_headers.py - no editar a mano.",
        "// Cada export salta a la funcion real de C:\\Windows\\System32\\winmm.dll.",
        "    .text",
    ]
    deff = ["LIBRARY winmm.dll", "EXPORTS"]
    for i, n in enumerate(names):
        asm += [f"    .globl stub_{n}", f"stub_{n}:", f"    jmp *g_winmm_real+{i * 8}(%rip)"]
        deff.append(f"    {n}=stub_{n}")
    header = [
        "// GENERADO por tools/gen_headers.py - no editar a mano.",
        "#pragma once",
        f"#define WINMM_EXPORT_COUNT {len(names)}",
        "static const char* const kWinmmExports[WINMM_EXPORT_COUNT] = {",
    ] + [f'    "{n}",' for n in names] + ["};", ""]
    open(os.path.join(SRC, "winmm_exports.h"), "w").write("\n".join(header))
    return "\n".join(asm) + "\n", "\n".join(deff) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", required=True)
    a = ap.parse_args()
    os.makedirs(SRC, exist_ok=True)
    open(os.path.join(SRC, "nms_sigs.h"), "w").write(gen_sigs(a.build))
    asm, deff = gen_winmm()
    open(os.path.join(SRC, "winmm_stubs.S"), "w").write(asm)
    open(os.path.join(SRC, "winmm.def"), "w").write(deff)
    print("ok: nms_sigs.h, winmm_exports.h, winmm_stubs.S, winmm.def")


if __name__ == "__main__":
    main()
