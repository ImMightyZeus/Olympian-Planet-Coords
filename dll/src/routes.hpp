// Olympian Planet Coords — rutas HTTP del overlay (iguales a las de la version Python con NMS.py).
// Olympian Planet Coords — overlay HTTP routes (same as the Python version under NMS.py).
//
// Olympian Planet Coords — Copyright © 2026 MightyZeus · GPL-3.0 License
#pragma once

#include "core.hpp"
#include "http_server.hpp"

namespace olympian {

constexpr const char* kRoutePrefix = "/OlympianPlanetCoords/";

// extra_debug: datos del DLL que se agregan a /debug (version del juego, funciones encontradas...).
inline HttpResponse Route(Core& core, const HttpRequest& req, const Json& extra_debug) {
    const std::string prefix = kRoutePrefix;
    if (req.path.compare(0, prefix.size(), prefix) != 0) return {404, "{\"detail\":\"Not Found\"}"};
    std::string route = req.path.substr(prefix.size());
    if (!route.empty() && route.back() == '/') route.pop_back();
    if (req.method == "GET") {
        if (route == "coords") return {200, core.CoordsJson()};
        if (route == "debug") return {200, core.DebugJson(extra_debug)};
        return {404, "{\"detail\":\"Not Found\"}"};
    }
    if (req.method == "PUT" || req.method == "POST") {
        if (route == "lang" || route == "hotkey" || route == "marker" || route == "clear" ||
            route == "forget") {
            Json q = Json::object();
            for (const auto& [k, v] : req.query) q[k] = v;
            return {200, core.Put(route, q)};
        }
        return {404, "{\"detail\":\"Not Found\"}"};
    }
    return {405, "{\"detail\":\"Method Not Allowed\"}"};
}

}  // namespace olympian
