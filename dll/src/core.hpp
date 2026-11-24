// Olympian Planet Coords — nucleo portable del DLL (datos, coordenadas, respuestas para el overlay).
// Olympian Planet Coords — portable DLL core (data, coordinates, responses for the overlay).
//
// Olympian Planet Coords — Copyright © 2026 MightyZeus · GPL-3.0 License
//
// Hace lo mismo que la version Python v0.0.3 (OlympianPlanetCoords.py), con el mismo datos.json y
// las mismas respuestas HTTP, para que el overlay funcione igual. No depende de Windows ni del
// juego: el DLL le entrega una "vista" del juego (GameView) en cada frame.
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <utility>
#include <string>

#include "geo.hpp"
#include "nlohmann/json.hpp"

namespace olympian {

using Json = nlohmann::ordered_json;

constexpr const char* kModVersion = "0.2.0-dev.8";  // v0.2 en desarrollo: digitos DSEG7 sin cortar
constexpr double kArrivalMeters = 50.0;
constexpr double kOrbitFactor = 3.0;
constexpr double kStableSeconds = 1.0;
constexpr int kHistoryMax = 10;       // destinos guardados por planeta
constexpr int kHistoryTotalMax = 300;  // entre todos los planetas
constexpr int kNameMaxChars = 60;   // como la version Python (name.strip()[:60])
constexpr int kMarkerTypeCustom = 0x35;  // cGcMarkerType::CustomMarker (nmspy 185034.0)

// Teclas que se pueden elegir (igual que HOTKEY_CHOICES de Python). "ninguna" = sin tecla.
extern const char* const kHotkeyChoices[8];
// Codigo de tecla virtual de Windows (VK_F5..VK_F11) o 0 para "ninguna"/desconocida.
int HotkeyVirtualKey(const std::string& name);

// Marcador que el DLL dibuja en el HUD con cGcBaseBuildingManager::AddHUDMarker (como el metodo
// BaseBuildingHUD de la version Python). Lo calcula Tick en cada frame.
struct MarkerSpec {
    bool valid = false;
    float local[3] = {0, 0, 0};   // cTkPhysRelVec3.local
    float offset[3] = {0, 0, 0};  // cTkPhysRelVec3.offset
    char label[64] = {0};         // cTkFixedString0x40 (UTF-8, siempre termina en 0)
};

// Recorta texto UTF-8 a max_bytes sin partir un caracter.
// Idiomas del mod: "es", "en" o "ru".
bool IsLang(const std::string& code);

std::string Utf8TruncateBytes(const std::string& s, size_t max_bytes);
// Recorta texto UTF-8 a max_chars caracteres.
std::string Utf8TruncateChars(const std::string& s, size_t max_chars);

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

// Nombre de la galaxia para RealityIndex (0-255); fuera de rango "#<numero>". Igual que galaxy_name().
std::string GalaxyName(int reality);
// Direccion universal de 64 bits como la arma el juego (igual que universe_address() de Python).
uint64_t UniverseAddress(int reality, int voxel_x, int voxel_y, int voxel_z, int system, int planet = 0);

// De donde salen los nombres del sistema y de la estacion (el DLL llama a funciones del juego; las
// pruebas ponen nombres fijos). Reciben la direccion universal del sistema (planeta 0). Se llaman desde
// Tick (hilo del juego), solo al cambiar de sistema o cada kNameRefreshSeconds.
struct NameSource {
    std::function<std::string(uint64_t ua)> system;
    std::function<std::string(uint64_t ua)> station;
};
constexpr double kNameRefreshSeconds = 3.0;

class Core {
   public:
    // data_path: ruta UTF-8 de datos.json. system_lang: "es", "en" o "ru" (idioma de Windows).
    Core(std::string data_path, std::string system_lang);

    // Llamar una vez por frame desde el hilo del juego. view == nullptr: no hay juego todavia.
    void Tick(const GameView* view, double now_seconds);

    // Respuestas HTTP (JSON). Seguras desde cualquier hilo.
    std::string CoordsJson();
    std::string DebugJson(const Json& extra);
    // PUT /lang, /hotkey, /marker, /clear. query: parametros ya decodificados.
    std::string Put(const std::string& route, const Json& query);

    std::string Lang();
    // Ajuste "ingame_box" de datos.json (por defecto si): dibujar el recuadro dentro del juego.
    bool InGameBox();
    const std::string& DataPath() const { return data_path_; }

    // Marcador del frame actual (para el hook de cGcBaseBuildingManager::Update). Seguro desde
    // cualquier hilo y no espera a las respuestas HTTP.
    MarkerSpec Marker();
    // Tecla configurada (codigo virtual de Windows; 0 = ninguna).
    int HotkeyVk() const { return hotkey_vk_.load(); }
    // Estado de la tecla en este frame. focused = el juego tiene el foco. Cuenta una pulsacion por
    // cada vez que la tecla pasa de suelta a presionada con el juego en primer plano.
    void KeyState(bool down, bool focused);
    // Datos de diagnostico del marcador (se agregan a /debug).
    void NoteMarkerCall(bool drawn);
    // Fuente de nombres de sistema y estacion (ver NameSource). Llamar antes del primer Tick.
    void SetNameSource(NameSource names);

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
    int input_request_id_ = 0;

    std::mutex marker_mu_;
    MarkerSpec marker_;
    std::atomic<int> hotkey_vk_{0};
    bool key_was_down_ = false;
    std::atomic<unsigned long long> marker_drawn_{0};
    std::atomic<unsigned long long> marker_skipped_{0};
    void UpdateHotkeyLocked();
    Json PlaceLocked(const GameView& v, double now);
    std::string CachedName(int kind, uint64_t ua, double now);
    NameSource names_;
    std::map<std::pair<int, uint64_t>, std::pair<std::string, double>> name_cache_;
    void SetMarkerLocked(const MarkerSpec& m);
};

}  // namespace olympian
