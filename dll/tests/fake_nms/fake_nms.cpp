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
        ++frames;
        Sleep(16);
    }
    printf("frames=%lu\n", frames);
    return 0;
}
