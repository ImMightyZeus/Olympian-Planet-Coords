// Banco de pruebas del nucleo del DLL (sin el juego). Lo maneja dll/tests/test_dll_core.py.
// Test harness for the DLL core (without the game). Driven by dll/tests/test_dll_core.py.
//
// Olympian Planet Coords — Copyright © 2026 MightyZeus · GPL-3.0 License
//
// Modos:
//   geo                 lee JSON por linea con funciones de geo y responde el resultado (stdin/stdout)
//   core <datos.json>   lee "frames" JSON por linea (GameView + tiempo) y responde CoordsJson()
//   serve <datos.json> <puerto>   servidor HTTP con un jugador simulado que camina (para el overlay)
//   fakegame <datos.json> <puerto> (solo Windows/Wine) arma en memoria las estructuras del juego con
//                       los desplazamientos reales y las lee con ReadGameView, como el DLL.

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "core.hpp"
#include "http_server.hpp"
#include "routes.hpp"

#ifdef _WIN32
#include "game_reader.hpp"
#include "nms_offsets.h"
#endif

using olympian::Json;

static geo::Vec3 V(const Json& j) { return {j[0].get<double>(), j[1].get<double>(), j[2].get<double>()}; }

static int RunGeo() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        Json in = Json::parse(line), out;
        std::string fn = in["fn"];
        if (fn == "v2ll") {
            double lat, lon;
            bool ok = geo::VectorToLatLon(V(in["rel"]), in["conv"], in["inv"], &lat, &lon);
            out = ok ? Json{lat, lon} : Json(nullptr);
        } else if (fn == "ll2u") {
            geo::Vec3 u = geo::LatLonToUnit(in["lat"], in["lon"], in["conv"], in["inv"]);
            out = Json{u.x, u.y, u.z};
        } else if (fn == "dist") {
            out = geo::SurfaceDistance(in["a"][0], in["a"][1], in["b"][0], in["b"][1], in["r"]);
        } else if (fn == "brg") {
            out = geo::InitialBearing(in["a"][0], in["a"][1], in["b"][0], in["b"][1]);
        } else if (fn == "card") {
            out = geo::Cardinal(in["b"], in["lang"]);
        } else if (fn == "reldir") {
            double y, p;
            geo::RelativeDirection(V(in["o"]), V(in["t"]), V(in["r"]), V(in["u"]), V(in["at"]), in["flip"], &y, &p);
            out = Json{y, p};
        } else if (fn == "round") {
            out = geo::RoundTo(in["v"], in["d"]);
        }
        std::cout << out.dump() << "\n" << std::flush;
    }
    return 0;
}

static olympian::GameView ViewFrom(const Json& f) {
    olympian::GameView v;
    v.have_player_env = f.value("have_player_env", true);
    v.location = f.value("location", 3);
    v.tm_right = V(f.value("right", Json{1, 0, 0}));
    v.tm_up = V(f.value("up", Json{0, 1, 0}));
    v.tm_at = V(f.value("at", Json{0, 0, 1}));
    v.tm_pos = V(f["pos"]);
    v.nearest_planet_pos = V(f["center"]);
    v.nearest_planet_index = f.value("planet_index", 1);
    v.have_address = f.value("have_address", true);
    v.reality = 0;
    v.voxel_x = 10;
    v.voxel_y = -3;
    v.voxel_z = 7;
    v.system = f.value("system", 42);
    v.have_player = true;
    return v;
}

static int RunCore(const std::string& path) {
    olympian::Core core(path, "es");
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        Json f = Json::parse(line);
        if (f.contains("put")) {
            Json q = f.value("query", Json::object());
            std::cout << core.Put(f["put"], q) << "\n" << std::flush;
            continue;
        }
        if (f.value("none", false)) {
            core.Tick(nullptr, f.value("t", 0.0));
        } else {
            olympian::GameView v = ViewFrom(f);
            core.Tick(&v, f.value("t", 0.0));
        }
        std::cout << core.CoordsJson() << "\n" << std::flush;
    }
    return 0;
}

// Jugador simulado que camina por un planeta de radio 3000 hacia el destino (como tests/sim_server.py).
static void SimulatedView(double t, olympian::GameView* v) {
    const double R = 3000.0;
    double lat = 12.0 + 2.0 * std::sin(t * 0.3), lon = -45.0 + t * 0.5;
    geo::Vec3 u = geo::LatLonToUnit(lat, lon, 0, false);
    olympian::Json f{{"pos", {100 + u.x * R, -50 + u.y * R, 20 + u.z * R}}, {"center", {100, -50, 20}}};
    *v = ViewFrom(f);
}

static int RunServe(const std::string& path, int port) {
    olympian::Core core(path, "es");
    olympian::HttpServer http;
    std::string err;
    if (!http.Start(port, [&](const olympian::HttpRequest& r) { return olympian::Route(core, r, Json::object()); }, &err)) {
        std::cerr << "HTTP: " << err << "\n";
        return 2;
    }
    std::cout << "serving " << port << "\n" << std::flush;
    auto t0 = std::chrono::steady_clock::now();
    for (;;) {
        double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        olympian::GameView v;
        SimulatedView(t, &v);
        core.Tick(&v, t);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

#ifdef _WIN32
// Arma en memoria un "cGcApplication" falso con los desplazamientos reales y deja que ReadGameView
// lo recorra igual que en el juego.
static int RunFakeGame(const std::string& path, int port) {
    using namespace nmsoff;
    const size_t data_size = DATA_SIMULATION + SIM_PLAYER + PLAYER_GRAPHICS_MATRIX + 0x100;
    std::vector<unsigned char> app(0x100, 0), data(data_size, 0);
    uintptr_t data_addr = reinterpret_cast<uintptr_t>(data.data());
    std::memcpy(app.data() + APP_MPDATA, &data_addr, sizeof(data_addr));
    unsigned char* penv = data.data() + DATA_SIMULATION + SIM_ENVIRONMENT + ENV_PLAYERENV;
    unsigned char* loca = data.data() + DATA_GAMESTATE + GAMESTATE_PLAYERSTATE + PSTATE_LOCATION;
    auto put3 = [](unsigned char* p, double x, double y, double z) {
        float f[3] = {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
        std::memcpy(p, f, sizeof(f));
    };
    auto put_i = [](unsigned char* p, int v) { std::memcpy(p, &v, sizeof(v)); };
    put_i(loca + LOC_REALITY_INDEX, 0);
    put_i(loca + LOC_GALACTIC_ADDRESS + GA_VOXEL_X, 10);
    put_i(loca + LOC_GALACTIC_ADDRESS + GA_VOXEL_Y, -3);
    put_i(loca + LOC_GALACTIC_ADDRESS + GA_VOXEL_Z, 7);
    put_i(loca + LOC_GALACTIC_ADDRESS + GA_SOLAR_SYSTEM_INDEX, 42);
    put_i(penv + PENV_NEAREST_PLANET_INDEX, 1);
    put_i(penv + PENV_LOCATION_STABLE, LOC_PLANETONFOOT);
    put3(penv + PENV_PLAYERTM + TM_RIGHT, 1, 0, 0);
    put3(penv + PENV_PLAYERTM + TM_UP, 0, 1, 0);
    put3(penv + PENV_PLAYERTM + TM_AT, 0, 0, 1);
    put3(penv + PENV_NEAREST_PLANET_POS, 100, -50, 20);

    // Sin mpData (menu / carga): debe responder "esperando al juego".
    std::vector<unsigned char> empty_app(0x100, 0);
    olympian::GameView probe;
    bool read_empty = olympian::ReadGameView(reinterpret_cast<uintptr_t>(empty_app.data()), &probe);
    std::cout << "empty_app_read=" << (read_empty ? 1 : 0) << "\n";

    olympian::Core core(path, "es");
    olympian::HttpServer http;
    std::string err;
    if (!http.Start(port, [&](const olympian::HttpRequest& r) { return olympian::Route(core, r, Json::object()); }, &err)) {
        std::cerr << "HTTP: " << err << "\n";
        return 2;
    }
    std::cout << "serving " << port << "\n" << std::flush;
    auto t0 = std::chrono::steady_clock::now();
    for (;;) {
        double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        const double R = 3000.0;
        double lat = 12.0 + 2.0 * std::sin(t * 0.3), lon = -45.0 + t * 0.5;
        geo::Vec3 u = geo::LatLonToUnit(lat, lon, 0, false);
        put3(penv + PENV_PLAYERTM + TM_POS, 100 + u.x * R, -50 + u.y * R, 20 + u.z * R);
        olympian::GameView v;
        bool ok = olympian::ReadGameView(reinterpret_cast<uintptr_t>(app.data()), &v);
        core.Tick(ok ? &v : nullptr, t);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}
#endif

int main(int argc, char** argv) {
    std::string mode = argc > 1 ? argv[1] : "";
    if (mode == "geo") return RunGeo();
    if (mode == "core" && argc > 2) return RunCore(argv[2]);
    if (mode == "serve" && argc > 3) return RunServe(argv[2], std::atoi(argv[3]));
#ifdef _WIN32
    if (mode == "fakegame" && argc > 3) return RunFakeGame(argv[2], std::atoi(argv[3]));
#endif
    std::cerr << "uso: core_harness geo | core <datos.json> | serve <datos.json> <puerto>"
#ifdef _WIN32
                 " | fakegame <datos.json> <puerto>"
#endif
              << "\n";
    return 1;
}
