// Generado por tools/gen_offsets.py desde nmspy 186923.0 (MIT). No editar a mano.
// Desplazamientos (bytes) de las estructuras del juego que lee Olympian Planet Coords.
#pragma once

namespace nmsoff {
constexpr unsigned long long APP_MPDATA = 0x38;  // 56
constexpr unsigned long long DATA_SIMULATION = 0x4cd620;  // 5035552
constexpr unsigned long long DATA_GAMESTATE = 0xeb0;  // 3760
constexpr unsigned long long SIM_ENVIRONMENT = 0xac930;  // 706864
constexpr unsigned long long SIM_PLAYER = 0x25f860;  // 2488416
constexpr unsigned long long SIM_SOLAR_SYSTEM = 0x25e090;  // 2482320
constexpr unsigned long long ENV_PLAYERENV = 0x8c0;  // 2240
constexpr unsigned long long PENV_PLAYERTM = 0x0;  // 0
constexpr unsigned long long PENV_NEAREST_PLANET_INDEX = 0x2bc;  // 700
constexpr unsigned long long PENV_DISTANCE_FROM_PLANET = 0x2c0;  // 704
constexpr unsigned long long PENV_NEAREST_PLANET_SEALEVEL = 0x2c4;  // 708
constexpr unsigned long long PENV_NEAREST_PLANET_POS = 0x2d0;  // 720
constexpr unsigned long long PENV_INSIDE_ATMOSPHERE = 0x2ec;  // 748
constexpr unsigned long long PENV_LOCATION_STABLE = 0x474;  // 1140
constexpr unsigned long long TM_RIGHT = 0x0;  // 0
constexpr unsigned long long TM_UP = 0x10;  // 16
constexpr unsigned long long TM_AT = 0x20;  // 32
constexpr unsigned long long TM_POS = 0x30;  // 48
constexpr unsigned long long GAMESTATE_PLAYERSTATE = 0xaad0;  // 43728
constexpr unsigned long long PSTATE_LOCATION = 0x860;  // 2144
constexpr unsigned long long LOC_GALACTIC_ADDRESS = 0x0;  // 0
constexpr unsigned long long LOC_REALITY_INDEX = 0x14;  // 20
constexpr unsigned long long GA_SOLAR_SYSTEM_INDEX = 0x4;  // 4
constexpr unsigned long long GA_VOXEL_X = 0x8;  // 8
constexpr unsigned long long GA_VOXEL_Y = 0xc;  // 12
constexpr unsigned long long GA_VOXEL_Z = 0x10;  // 16
constexpr unsigned long long PLAYER_GRAPHICS_MATRIX = 0x310;  // 784
constexpr unsigned long long GM_POS = 0x30;  // 48
constexpr unsigned long long REL_LOCAL = 0x0;  // 0
constexpr unsigned long long REL_OFFSET = 0x10;  // 16

// EnvironmentLocation::Enum
constexpr int LOC_NONE = 0;
constexpr int LOC_DEFAULT = 1;
constexpr int LOC_SPACESTATION = 2;
constexpr int LOC_PLANETONFOOT = 3;
constexpr int LOC_PLANETINSHIP = 4;
constexpr int LOC_PLANETINVEHICLE = 5;
constexpr int LOC_UNDERWATER = 6;
constexpr int LOC_CAVE = 7;
constexpr int LOC_INDOORINBASE = 8;
constexpr int LOC_FREIGHTER = 9;
constexpr int LOC_FREIGHTERINTERNALS = 10;
constexpr int LOC_ABANDONEDFREIGHTER = 11;
constexpr int LOC_INFLEET = 12;
constexpr int LOC_INSPACEOBJECT = 13;
constexpr int LOC_NEXUS = 14;
constexpr int LOC_ANOMALY = 15;
}  // namespace nmsoff
