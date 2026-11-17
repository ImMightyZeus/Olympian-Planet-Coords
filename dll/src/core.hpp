// Olympian Planet Coords — nucleo portable del DLL (datos, coordenadas, respuestas para el overlay).
// Olympian Planet Coords — portable DLL core (data, coordinates, responses for the overlay).
//
// Olympian Planet Coords — Copyright © 2026 MightyZeus · GPL-3.0 License
//
// Hace lo mismo que la version Python v0.0.3 (OlympianPlanetCoords.py), con el mismo datos.json y
// las mismas respuestas HTTP, para que el overlay funcione igual. No depende de Windows ni del
// juego: el DLL le entrega una "vista" del juego (GameView) en cada frame.
#pragma once

#include <mutex>
#include <string>

#include "geo.hpp"
#include "nlohmann/json.hpp"

namespace olympian {

using Json = nlohmann::ordered_json;

constexpr const char* kModVersion = "0.1.0-dll.3";  // paso 2: coordenadas en vivo (correccion)
constexpr double kArrivalMeters = 50.0;
constexpr double kOrbitFactor = 3.0;
constexpr double kStableSeconds = 1.0;
constexpr int kHistoryMax = 10;

// Lo que el DLL lee de la memoria del juego en un frame (mismos campos que usa la version Python).
struct GameView {
    bool have_player_env = false;  // false = "esperando al juego"
    int location = -1;             // EnvironmentLocation::Enum (meLocationStable)
    geo::Vec3 tm_right, tm_up, tm_at, tm_pos;  // cGcPlayerEnvironment.mPlayerTM
    geo::Vec3 nearest_planet_pos;              // mNearestPlanetPos
    int nearest_planet_index = 0;
    float distance_from_planet = 0, sealevel = 0;
    bool inside_atmosphere = false;
    bool have_player = false;
    geo::Vec3 graphics_local, graphics_offset;  // cGcPlayer.mGraphicsMatrix.pos (local / offset)
    bool have_address = false;
    int reality = 0, voxel_x = 0, voxel_y = 0, voxel_z = 0, system = 0;
};

std::string Tr(const std::string& key, const std::string& lang);

class Core {
   public:
    // data_path: ruta UTF-8 de datos.json. system_lang: "es" o "en" (idioma de Windows).
    Core(std::string data_path, std::string system_lang);

    // Llamar una vez por frame desde el hilo del juego. view == nullptr: no hay juego todavia.
    void Tick(const GameView* view, double now_seconds);

    // Respuestas HTTP (JSON). Seguras desde cualquier hilo.
    std::string CoordsJson();
    std::string DebugJson(const Json& extra);
    // PUT /lang, /hotkey, /marker, /clear. query: parametros ya decodificados.
    std::string Put(const std::string& route, const Json& query);

    std::string Lang();
    const std::string& DataPath() const { return data_path_; }

   private:
    void Load();
    void Save();
    std::string LangLocked() const;
    std::string T(const std::string& key) const { return Tr(key, LangLocked()); }
    std::string PlanetKey(const GameView& v) const;
    void SetResult(bool ok, const std::string& message);
    int SettingInt(const char* key, int def) const;
    bool SettingBool(const char* key, bool def) const;

    std::mutex mu_;
    std::string data_path_;
    std::string system_lang_;
    Json data_;
    Json snapshot_;
    Json last_result_;
    Json context_;
    std::string stable_key_;
    double stable_since_ = 0;
};

}  // namespace olympian
