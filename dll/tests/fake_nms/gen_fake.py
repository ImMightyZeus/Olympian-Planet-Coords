"""Genera sigs.S: una funcion falsa cGcApplication::Update (con el mismo prologo del juego) y los
demas patrones de NMS.py como bytes en .text, para probar el DLL completo bajo Wine sin el juego."""
import json, os, sys
here = os.path.dirname(os.path.abspath(__file__))
data = json.load(open(os.path.join(here, "..", "..", "tools", "nmspy_data_185034.json"), encoding="utf-8"))
sys.path.insert(0, os.path.join(here, "..", "..", "tools"))
from gen_headers import FUNCTIONS
by = {f["name"]: f["signature"].split() for f in data["functions"]}
out = ["    .text", "    .globl FakeHelper", "FakeHelper:", "    ret", ""]
# cGcApplication::Update: 40 53 48 83 EC 20 E8 ? ? ? ? 48 89  -> push rbx; sub rsp,20h; call; mov rbx,rcx
out += ["    .globl FakeUpdate", "FakeUpdate:", "    .byte 0x40, 0x53", "    .byte 0x48, 0x83, 0xEC, 0x20",
        "    call FakeHelper", "    .byte 0x48, 0x89, 0xCB", "    .byte 0x48, 0x83, 0xC4, 0x20", "    .byte 0x5B",
        "    ret", ""]
# cTkFSM::StateChange: 48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC ? 4C 8B 51 ? 49 8B E8
out += ["    .globl FakeStateChange", "FakeStateChange:",
        "    .byte 0x48, 0x89, 0x6C, 0x24, 0x10", "    .byte 0x48, 0x89, 0x74, 0x24, 0x18", "    .byte 0x57",
        "    .byte 0x48, 0x83, 0xEC, 0x20", "    .byte 0x4C, 0x8B, 0x51, 0x08", "    .byte 0x49, 0x8B, 0xE8",
        "    .byte 0x48, 0x83, 0xC4, 0x20", "    .byte 0x5F", "    .byte 0x48, 0x8B, 0x6C, 0x24, 0x10",
        "    .byte 0x48, 0x8B, 0x74, 0x24, 0x18", "    ret", ""]
out += ["    .globl FakeSignatures", "FakeSignatures:"]
for name in FUNCTIONS[1:]:
    if name == "cTkFSM::StateChange":
        continue
    bs = ["0xCC" if b.startswith("?") else "0x" + b for b in by[name]]
    out.append(f"    # {name}")
    out.append("    .byte " + ", ".join(bs))
    out.append("    .byte 0xCC, 0xCC, 0xCC, 0xCC")
out.append("    ret")
open(os.path.join(here, "sigs.S"), "w").write("\n".join(out) + "\n")
print("ok")
