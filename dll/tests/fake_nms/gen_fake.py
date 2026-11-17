"""Genera sigs.S: funciones falsas cGcApplication::Update, cTkFSM::StateChange,
cGcBaseBuildingManager::Update y AddHUDMarker (con el mismo prologo del juego) y los
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
# cGcBaseBuildingManager::Update (mismo prologo que el juego; llama a FakeBbmBody(this, dt))
# 48 8B C4 F3 0F 11 48 ? 55 53 56 57 41 54 41 55 41 56 41 57 48 8D A8 ? ? ? ? 48 81 EC ? ? ? ? 80 79
out += ["    .globl FakeBbmUpdate", "FakeBbmUpdate:",
        "    .byte 0x48, 0x8B, 0xC4", "    .byte 0xF3, 0x0F, 0x11, 0x48, 0x08",
        "    .byte 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57",
        "    .byte 0x48, 0x8D, 0xA8, 0xD8, 0xFF, 0xFF, 0xFF",  # lea rbp, [rax-0x28]
        "    .byte 0x48, 0x81, 0xEC, 0x28, 0x00, 0x00, 0x00",  # sub rsp, 0x28
        "    .byte 0x80, 0x79, 0x10, 0x00",  # cmp byte ptr [rcx+0x10], 0
        "    call FakeBbmBody",
        "    add $0x28, %rsp",
        "    pop %r15", "    pop %r14", "    pop %r13", "    pop %r12", "    pop %rdi", "    pop %rsi",
        "    pop %rbx", "    pop %rbp", "    ret", ""]
# cGcBaseBuildingManager::AddHUDMarker (mismo prologo; llama a FakeAddHudBody con los 5 argumentos)
# 48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 48 81 EC ? ? ? ? 48 8B E9 49 63 F1
out += ["    .globl FakeAddHudMarker", "FakeAddHudMarker:",
        "    .byte 0x48, 0x89, 0x5C, 0x24, 0x08", "    .byte 0x48, 0x89, 0x6C, 0x24, 0x10",
        "    .byte 0x48, 0x89, 0x74, 0x24, 0x18", "    .byte 0x57",
        "    .byte 0x48, 0x81, 0xEC, 0x30, 0x00, 0x00, 0x00",  # sub rsp, 0x30
        "    .byte 0x48, 0x8B, 0xE9", "    .byte 0x49, 0x63, 0xF1",  # mov rbp, rcx; movsxd rsi, r9d
        "    mov 0x60(%rsp), %rax",  # 5o argumento (nombre): entrada rsp+0x28 -> +0x30+8
        "    mov %rax, 0x20(%rsp)",
        "    call FakeAddHudBody",
        "    add $0x30, %rsp", "    pop %rdi",
        "    mov 0x8(%rsp), %rbx", "    mov 0x10(%rsp), %rbp", "    mov 0x18(%rsp), %rsi", "    ret", ""]
out += ["    .globl FakeSignatures", "FakeSignatures:"]
for name in FUNCTIONS[1:]:
    if name in ("cTkFSM::StateChange", "cGcBaseBuildingManager::Update", "cGcBaseBuildingManager::AddHUDMarker"):
        continue
    bs = ["0xCC" if b.startswith("?") else "0x" + b for b in by[name]]
    out.append(f"    # {name}")
    out.append("    .byte " + ", ".join(bs))
    out.append("    .byte 0xCC, 0xCC, 0xCC, 0xCC")
out.append("    ret")
open(os.path.join(here, "sigs.S"), "w").write("\n".join(out) + "\n")
print("ok")
