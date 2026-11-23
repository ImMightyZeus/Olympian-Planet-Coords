// Olympian Planet Coords — fuentes del recuadro dentro del juego, incluidas tal cual en el DLL.
// Olympian Planet Coords — fonts of the in-game box, embedded as-is in the DLL.
// DSEG7 Classic (keshikan) y Share Tech Mono (Carrois Apostrophe): SIL Open Font License 1.1
// (textos en overlay/fonts/*-OFL.txt). Son las mismas que usa el recuadro .exe.
// Olympian Planet Coords — Copyright © 2026 MightyZeus · GPL-3.0 License
    .section .rdata,"dr"
    .balign 16
    .globl olympian_font_dseg7
olympian_font_dseg7:
    .incbin "../overlay/fonts/DSEG7Classic-Bold.ttf"
    .globl olympian_font_dseg7_end
olympian_font_dseg7_end:
    .balign 16
    .globl olympian_font_sharetech
olympian_font_sharetech:
    .incbin "../overlay/fonts/ShareTechMono-Regular.ttf"
    .globl olympian_font_sharetech_end
olympian_font_sharetech_end:
