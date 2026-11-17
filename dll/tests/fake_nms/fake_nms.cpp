// "NMS.exe" falso para probar el DLL completo bajo Wine: version 185034, los patrones de las
// funciones del juego en .text y un cGcApplication armado en memoria con los desplazamientos reales.
// Fake "NMS.exe" to test the whole DLL under Wine.
// Olympian Planet Coords — Copyright © 2026 MightyZeus · GPL-3.0 License
#include <windows.h>
#include <mmsystem.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "../../src/nms_offsets.h"

extern "C" void FakeUpdate(void* app);
extern "C" void FakeStateChange(void* fsm, void* state, void* data, unsigned long long force);
extern "C" void FakeSignatures();
extern "C" void FakeBbmUpdate(void* bbm, float dt);
extern "C" void FakeAddHudMarker(void* bbm, int type, const void* mat, int colour, const char* name);

// cGcBaseBuildingManager falso: this+0x60 -> objeto con +0x1e8 -> tabla de colores (como el juego).
static unsigned char g_bbm[0x100];
static unsigned char g_bbm_inner[0x200];
static unsigned char g_colours[0x100];
static unsigned char g_bbm_bad[0x100];  // sin +0x60: el DLL no debe llamar a AddHUDMarker
static unsigned long g_hud_calls = 0, g_hud_bad_this = 0, g_bbm_body = 0;
static int g_hud_type = -1, g_hud_colour = -1;
static float g_hud_local[3], g_hud_offset[3], g_hud_right[3];
static char g_hud_name[65];

extern "C" void FakeBbmBody(void* self, float dt) {
    (void)self;
    (void)dt;
    ++g_bbm_body;
}

extern "C" void FakeAddHudBody(void* self, int type, const void* mat, int colour, const char* name) {
    if (self != g_bbm) ++g_hud_bad_this;
    ++g_hud_calls;
    g_hud_type = type & 0xFF;  // el juego solo usa dl
    g_hud_colour = colour;
    const float* m = (const float*)mat;
    memcpy(g_hud_right, m, sizeof(g_hud_right));
    memcpy(g_hud_local, m + 12, sizeof(g_hud_local));   // +0x30
    memcpy(g_hud_offset, m + 16, sizeof(g_hud_offset));  // +0x40
    memcpy(g_hud_name, name, 64);
    g_hud_name[64] = 0;
}

static unsigned char g_app[0x100];
static unsigned char g_decoy[0x100];  // otro cTkFSM (no es la aplicacion)
static unsigned char g_decoy_data[0x1000];
static unsigned char* g_data;

static void put3(unsigned char* p, double x, double y, double z) {
    float f[3] = {(float)x, (float)y, (float)z};
    memcpy(p, f, sizeof(f));
}
static void puti(unsigned char* p, int v) { memcpy(p, &v, 4); }

int main(int argc, char** argv) {
    using namespace nmsoff;
    double seconds = argc > 1 ? atof(argv[1]) : 10.0;
    DWORD t0 = timeGetTime();  // importa winmm.dll: Windows carga el proxy de Olympian
    printf("timeGetTime=%lu\n", (unsigned long)t0);
    fflush(stdout);
    volatile void* keep = (void*)&FakeSignatures;
    (void)keep;
    size_t size = DATA_SIMULATION + SIM_PLAYER + PLAYER_GRAPHICS_MATRIX + 0x100;
    g_data = (unsigned char*)VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    unsigned char* penv = g_data + DATA_SIMULATION + SIM_ENVIRONMENT + ENV_PLAYERENV;
    unsigned char* loca = g_data + DATA_GAMESTATE + GAMESTATE_PLAYERSTATE + PSTATE_LOCATION;
    puti(loca + LOC_REALITY_INDEX, 0);
    puti(loca + LOC_GALACTIC_ADDRESS + GA_VOXEL_X, 10);
    puti(loca + LOC_GALACTIC_ADDRESS + GA_VOXEL_Y, -3);
    puti(loca + LOC_GALACTIC_ADDRESS + GA_VOXEL_Z, 7);
    puti(loca + LOC_GALACTIC_ADDRESS + GA_SOLAR_SYSTEM_INDEX, 42);
    puti(penv + PENV_NEAREST_PLANET_INDEX, 1);
    puti(penv + PENV_LOCATION_STABLE, LOC_PLANETONFOOT);
    put3(penv + PENV_PLAYERTM + TM_RIGHT, 1, 0, 0);
    put3(penv + PENV_PLAYERTM + TM_UP, 0, 1, 0);
    put3(penv + PENV_PLAYERTM + TM_AT, 0, 0, 1);
    put3(penv + PENV_NEAREST_PLANET_POS, 100, -50, 20);

    // Como el juego: varios cTkFSM cambian de estado; uno es la aplicacion. El "otro" apunta a datos
    // vacios y no debe confundirse con la aplicacion.
    uintptr_t dd = (uintptr_t)g_decoy_data;
    memcpy(g_decoy + APP_MPDATA, &dd, sizeof(dd));
    uintptr_t inner = (uintptr_t)g_bbm_inner, colours = (uintptr_t)g_colours;
    memcpy(g_bbm + 0x60, &inner, sizeof(inner));
    memcpy(g_bbm_inner + 0x1e8, &colours, sizeof(colours));
    volatile void* keep2 = (void*)&FakeAddHudMarker;
    (void)keep2;
    Sleep(1500);  // el DLL instala sus hooks al cargar
    FakeStateChange(g_decoy, nullptr, nullptr, 0);
    FakeStateChange(g_app, nullptr, nullptr, 0);

    // Primeros 2 s: menu (sin mpData). Despues: jugador caminando en un planeta de radio 3000.
    DWORD start = GetTickCount();
    unsigned long frames = 0;
    for (;;) {
        double t = (GetTickCount() - start) / 1000.0;
        if (t > seconds) break;
        if (t > 2.0) {
            uintptr_t d = (uintptr_t)g_data;
            memcpy(g_app + APP_MPDATA, &d, sizeof(d));
            double lat = 12.0 + 2.0 * sin(t * 0.3), lon = -45.0 + t * 0.5;
            double la = lat * 3.14159265358979 / 180, lo = lon * 3.14159265358979 / 180;
            double R = 3000;
            put3(penv + PENV_PLAYERTM + TM_POS, 100 + cos(la) * sin(lo) * R, -50 + sin(la) * R, 20 + cos(la) * cos(lo) * R);
        }
        FakeUpdate((void*)0x1234);  // como el juego real: rcx NO es la aplicacion
        FakeBbmUpdate(g_bbm_bad, 0.016f);
        FakeBbmUpdate(g_bbm, 0.016f);
        ++frames;
        Sleep(16);
    }
    printf("frames=%lu\n", frames);
    printf("bbm_body=%lu hud_calls=%lu hud_bad_this=%lu type=0x%x colour=%d\n", g_bbm_body, g_hud_calls,
           g_hud_bad_this, g_hud_type, g_hud_colour);
    printf("hud_local=%.3f,%.3f,%.3f hud_offset=%.3f,%.3f,%.3f hud_right=%.1f,%.1f,%.1f\n", g_hud_local[0],
           g_hud_local[1], g_hud_local[2], g_hud_offset[0], g_hud_offset[1], g_hud_offset[2], g_hud_right[0],
           g_hud_right[1], g_hud_right[2]);
    printf("hud_name=%s\n", g_hud_name);
    fflush(stdout);
    return 0;
}
