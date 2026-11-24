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
#include "ingame_ui.hpp"
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
    v.reality = f.value("reality", 0);
    v.voxel_x = 10;
    v.voxel_y = -3;
    v.voxel_z = 7;
    v.system = f.value("system", 42);
    v.have_player = true;
    return v;
}

static int RunCore(const std::string& path, const std::string& system_lang = "es") {
    olympian::Core core(path, system_lang);
    int name_calls = 0;
    olympian::Panel panel(olympian::PanelHost{
        [&core] { return core.CoordsJson(); },
        [&core](const std::string& r, const Json& q) { return core.Put(r, q); }});
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        Json f = Json::parse(line);
        if (f.contains("put")) {
            Json q = f.value("query", Json::object());
            std::cout << core.Put(f["put"], q) << "\n" << std::flush;
            continue;
        }
        if (f.contains("parse")) {  // lectura de coordenadas escritas (igual que parse_coords de Python)
            olympian::ParsedCoords p = olympian::ParseCoords(f["parse"].get<std::string>());
            std::cout << Json{{"ok", p.ok}, {"lat", p.lat}, {"lon", p.lon}, {"name", p.name}, {"error", p.error}}.dump()
                      << "\n" << std::flush;
            continue;
        }
        if (f.contains("panel")) {  // panel: "open", "close", "toggle" y teclas
            std::string cmd = f["panel"].get<std::string>();
            if (cmd == "open") panel.Open();
            if (cmd == "close") panel.Close();
            if (cmd == "toggle") panel.Toggle();
            if (f.contains("keys"))
                for (const auto& k : f["keys"]) {
                    std::string n = k.get<std::string>();
                    olympian::UiEvent e;
                    if (n == "Enter") e.key = olympian::UiKey::Enter;
                    else if (n == "Esc") e.key = olympian::UiKey::Esc;
                    else if (n == "Back") e.key = olympian::UiKey::Back;
                    else if (n == "Up") e.key = olympian::UiKey::Up;
                    else if (n == "Down") e.key = olympian::UiKey::Down;
                    else if (n == "Left") e.key = olympian::UiKey::Left;
                    else if (n == "Right") e.key = olympian::UiKey::Right;
                    else if (n == "Del") e.key = olympian::UiKey::Del;
                    else if (n == "Tab") e.key = olympian::UiKey::Tab;
                    else {  // texto: una letra por evento (UTF-8 -> codigo)
                        std::u32string u;
                        for (size_t i = 0; i < n.size();) {
                            unsigned char c = static_cast<unsigned char>(n[i]);
                            int len = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
                            char32_t cp = len == 1 ? c : len == 2 ? c & 0x1F : len == 3 ? c & 0x0F : c & 0x07;
                            for (int j = 1; j < len; ++j) cp = (cp << 6) | (static_cast<unsigned char>(n[i + j]) & 0x3F);
                            u += cp;
                            i += static_cast<size_t>(len);
                        }
                        for (char32_t cp : u) panel.OnKey(olympian::UiEvent{olympian::UiKey::Char, cp});
                        continue;
                    }
                    panel.OnKey(e);
                }
            std::cout << Json{{"open", panel.IsOpen()}, {"input", panel.InputUtf8()}, {"sel", panel.Selected()},
                              {"page", panel.Page()}, {"text", panel.Text()}}
                             .dump(-1, ' ', false, Json::error_handler_t::replace)
                      << "\n" << std::flush;
            continue;
        }
        if (f.contains("names")) {  // nombres fijos de sistema y estacion (en el DLL los da el juego)
            std::string sys = f["names"].value("system", ""), st = f["names"].value("station", "");
            core.SetNameSource(olympian::NameSource{
                [&name_calls, sys](uint64_t) { ++name_calls; return sys; },
                [&name_calls, st](uint64_t ua) {
                    ++name_calls;
                    char buf[32];
                    std::snprintf(buf, sizeof(buf), "%llx", static_cast<unsigned long long>(ua));
                    return st == "ua" ? std::string(buf) : st;
                }});
            std::cout << Json{{"ok", true}}.dump() << "\n" << std::flush;
            continue;
        }
        if (f.contains("name_calls")) {
            std::cout << Json{{"name_calls", name_calls}}.dump() << "\n" << std::flush;
            continue;
        }
        if (f.contains("galaxy")) {  // tabla de galaxias y direccion universal
            Json out = Json::array();
            for (int i = -1; i <= 256; ++i) out.push_back(olympian::GalaxyName(i));
            const Json& a = f["galaxy"];
            uint64_t ua = olympian::UniverseAddress(a[0], a[1], a[2], a[3], a[4], a[5]);
            std::cout << Json{{"names", out}, {"ua", std::to_string(ua)}}.dump() << "\n" << std::flush;
            continue;
        }
        if (f.contains("key")) {  // estado de la tecla en un frame
            core.KeyState(f["key"].get<bool>(), f.value("focused", true));
            std::cout << core.CoordsJson() << "\n" << std::flush;
            continue;
        }
        if (f.value("marker", false)) {  // marcador que dibujaria el hook del HUD
            olympian::MarkerSpec m = core.Marker();
            Json o{{"valid", m.valid},
                   {"local", {m.local[0], m.local[1], m.local[2]}},
                   {"offset", {m.offset[0], m.offset[1], m.offset[2]}},
                   {"label", std::string(m.label)},
                   {"vk", core.HotkeyVk()}};
            std::cout << o.dump() << "\n" << std::flush;
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
    if (mode == "core" && argc > 2) return RunCore(argv[2], argc > 3 ? argv[3] : "es");
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
