// Olympian Planet Coords — lectura segura de la memoria del juego. Ver game_reader.hpp.
// Olympian Planet Coords — Copyright © 2026 MightyZeus · GPL-3.0 License

#include "game_reader.hpp"

#include <windows.h>

#include <cmath>

#include "nms_offsets.h"

namespace olympian {

namespace {

template <typename T>
bool Rd(uintptr_t addr, T* out) {
    if (addr < 0x10000) return false;
    SIZE_T got = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(addr), out, sizeof(T), &got) &&
           got == sizeof(T);
}

bool RdVec3(uintptr_t addr, geo::Vec3* v) {
    float f[3];
    if (!Rd(addr, &f)) return false;
    v->x = f[0];
    v->y = f[1];
    v->z = f[2];
    return true;
}

}  // namespace

uintptr_t ReadPointer(uintptr_t addr) {
    uintptr_t v = 0;
    return Rd(addr, &v) ? v : 0;
}

bool PlausibleView(const GameView& v) {
    if (!v.have_player_env || v.location < 0 || v.location > 15) return false;
    auto unit = [](const geo::Vec3& a) {
        double l = geo::Length(a);
        return std::isfinite(l) && l > 0.9 && l < 1.1;
    };
    auto finite = [](const geo::Vec3& a) {
        return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z) && std::fabs(a.x) < 1e9 &&
               std::fabs(a.y) < 1e9 && std::fabs(a.z) < 1e9;
    };
    return unit(v.tm_right) && unit(v.tm_up) && unit(v.tm_at) && finite(v.tm_pos) && finite(v.nearest_planet_pos);
}

bool ReadGameView(uintptr_t app, GameView* out) {
    using namespace nmsoff;
    *out = GameView{};
    uintptr_t data = 0;
    if (!Rd(app + APP_MPDATA, &data) || data < 0x10000) return false;
    uintptr_t sim = data + DATA_SIMULATION;
    uintptr_t penv = sim + SIM_ENVIRONMENT + ENV_PLAYERENV;

    int loc = -1;
    if (!Rd(penv + PENV_LOCATION_STABLE, &loc)) return false;
    if (!RdVec3(penv + PENV_PLAYERTM + TM_RIGHT, &out->tm_right) || !RdVec3(penv + PENV_PLAYERTM + TM_UP, &out->tm_up) ||
        !RdVec3(penv + PENV_PLAYERTM + TM_AT, &out->tm_at) || !RdVec3(penv + PENV_PLAYERTM + TM_POS, &out->tm_pos) ||
        !RdVec3(penv + PENV_NEAREST_PLANET_POS, &out->nearest_planet_pos) ||
        !Rd(penv + PENV_NEAREST_PLANET_INDEX, &out->nearest_planet_index) ||
        !Rd(penv + PENV_DISTANCE_FROM_PLANET, &out->distance_from_planet) ||
        !Rd(penv + PENV_NEAREST_PLANET_SEALEVEL, &out->sealevel))
        return false;
    uint8_t atm = 0;
    Rd(penv + PENV_INSIDE_ATMOSPHERE, &atm);
    out->inside_atmosphere = atm != 0;
    out->location = loc;
    out->have_player_env = true;

    uintptr_t gpos = sim + SIM_PLAYER + PLAYER_GRAPHICS_MATRIX + GM_POS;
    out->have_player = RdVec3(gpos + REL_LOCAL, &out->graphics_local) && RdVec3(gpos + REL_OFFSET, &out->graphics_offset);

    uintptr_t loca = data + DATA_GAMESTATE + GAMESTATE_PLAYERSTATE + PSTATE_LOCATION;
    uintptr_t ga = loca + LOC_GALACTIC_ADDRESS;
    out->have_address = Rd(loca + LOC_REALITY_INDEX, &out->reality) && Rd(ga + GA_VOXEL_X, &out->voxel_x) &&
                        Rd(ga + GA_VOXEL_Y, &out->voxel_y) && Rd(ga + GA_VOXEL_Z, &out->voxel_z) &&
                        Rd(ga + GA_SOLAR_SYSTEM_INDEX, &out->system);
    return true;
}

}  // namespace olympian
