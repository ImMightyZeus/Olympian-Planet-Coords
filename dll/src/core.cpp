// Olympian Planet Coords — nucleo portable del DLL. Ver core.hpp.
// Olympian Planet Coords — Copyright © 2026 MightyZeus · GPL-3.0 License

#include "core.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace olympian {

namespace {

const std::map<std::string, std::pair<const char*, const char*>> kMessages = {
    {"waiting", {"esperando al juego", "waiting for the game"}},
    {"not_on_planet", {"no estas en un planeta", "you're not on a planet"}},
    {"arrived", {"Llegaste a {name}", "You reached {name}"}},
    {"bad_lang", {"idioma no valido", "invalid language"}},
    {"dll_not_yet",
     {"esta funcion llega en la siguiente version del DLL", "this feature arrives in the next DLL version"}},
    {"error", {"error: {err}", "error: {err}"}},
};

bool IsPlanetLocation(int loc) {
    // PlanetOnFoot, PlanetInShip, PlanetInVehicle, Underwater, Cave, IndoorInBase
    return loc >= 3 && loc <= 8;
}
constexpr int kLocDefault = 1;  // en el espacio (orbita posible)

std::string FormatCoords(double lat, double lon) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%+.2f, %+.2f", lat, lon);
    return buf;
}

std::string Replace(std::string s, const std::string& from, const std::string& to) {
    size_t p = s.find(from);
    if (p != std::string::npos) s.replace(p, from.size(), to);
    return s;
}

fs::path PathOf(const std::string& utf8) {
#if defined(_WIN32) && __cplusplus >= 202002L
    return fs::path(std::u8string(utf8.begin(), utf8.end()));
#else
    return fs::u8path(utf8);
#endif
}

}  // namespace

std::string Tr(const std::string& key, const std::string& lang) {
    auto it = kMessages.find(key);
    if (it == kMessages.end()) return key;
    return lang == "en" ? it->second.second : it->second.first;
}

Core::Core(std::string data_path, std::string system_lang)
    : data_path_(std::move(data_path)), system_lang_(system_lang == "en" ? "en" : "es") {
    last_result_ = Json{{"ok", nullptr}, {"message", ""}, {"seq", 0}};
    Load();
    snapshot_ = Json{{"ok", true},         {"on_planet", false}, {"in_orbit", false},
                     {"reason", T("waiting")}, {"lat", nullptr},     {"lon", nullptr},
                     {"destination", nullptr}};
}

// ---- datos.json (mismo formato que la version Python) -----------------------------------------
void Core::Load() {
    data_ = Json{{"version", 1},
                 {"destinations", Json::object()},
                 {"history", Json::array()},
                 {"radii", Json::object()},
                 {"settings",
                  Json{{"convention", 0},
                       {"invert_lat", false},
                       {"planet_pos_is_local", false},
                       {"method", 1},
                       {"pos_source", 0},
                       {"env_add_offset", false},
                       {"arrow_flip", false},
                       {"hotkey", "f9"},
                       {"lang", nullptr}}}};
    fs::path p = PathOf(data_path_);
    std::ifstream f(p, std::ios::binary);
    if (!f) return;  // todavia no existe
    try {
        Json raw = Json::parse(f);
        if (raw.contains("destinations") && raw["destinations"].is_object()) data_["destinations"] = raw["destinations"];
        if (raw.contains("history") && raw["history"].is_array()) {
            Json h = Json::array();
            for (size_t i = 0; i < raw["history"].size() && i < static_cast<size_t>(kHistoryMax); ++i)
                h.push_back(raw["history"][i]);
            data_["history"] = h;
        }
        if (raw.contains("settings") && raw["settings"].is_object())
            for (auto& [k, v] : raw["settings"].items()) data_["settings"][k] = v;
        if (raw.contains("radii") && raw["radii"].is_object()) data_["radii"] = raw["radii"];
    } catch (...) {
        f.close();
        std::error_code ec;
        fs::rename(p, PathOf(data_path_ + ".bak"), ec);  // archivo danado: se guarda una copia
    }
}

void Core::Save() {
    fs::path p = PathOf(data_path_);
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    fs::path tmp = PathOf(data_path_ + ".tmp");
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return;
        f << data_.dump(1, ' ', false, Json::error_handler_t::replace);
    }
#ifdef _WIN32
    MoveFileExW(tmp.c_str(), p.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
#else
    fs::rename(tmp, p, ec);
#endif
}

int Core::SettingInt(const char* key, int def) const {
    const Json& s = data_["settings"];
    if (!s.contains(key)) return def;
    const Json& v = s[key];
    if (v.is_number_integer()) return v.get<int>();
    if (v.is_boolean()) return v.get<bool>() ? 1 : 0;
    return def;
}

bool Core::SettingBool(const char* key, bool def) const {
    const Json& s = data_["settings"];
    if (!s.contains(key)) return def;
    const Json& v = s[key];
    if (v.is_boolean()) return v.get<bool>();
    if (v.is_number_integer()) return v.get<int>() != 0;
    return def;
}

std::string Core::LangLocked() const {
    const Json& s = data_["settings"];
    if (s.contains("lang") && s["lang"].is_string()) {
        std::string l = s["lang"].get<std::string>().substr(0, 2);
        if (l == "es" || l == "en") return l;
    }
    return system_lang_;
}

std::string Core::Lang() {
    std::lock_guard<std::mutex> g(mu_);
    return LangLocked();
}

std::string Core::PlanetKey(const GameView& v) const {
    if (!v.have_address) return "P" + std::to_string(v.nearest_planet_index);
    std::ostringstream o;
    o << "G" << v.reality << ":" << v.voxel_x << ":" << v.voxel_y << ":" << v.voxel_z << ":" << v.system << ":P"
      << v.nearest_planet_index;
    return o.str();
}

void Core::SetResult(bool ok, const std::string& message) {
    int seq = last_result_.value("seq", 0) + 1;
    last_result_ = Json{{"ok", ok}, {"message", message}, {"seq", seq}};
}

// ---- un frame del juego (port de _read_context + _update) --------------------------------------
void Core::Tick(const GameView* view, double now) {
    std::lock_guard<std::mutex> g(mu_);
    auto not_ready = [&](const std::string& reason) {
        context_ = nullptr;
        stable_key_.clear();
        snapshot_ = Json{{"ok", true},         {"on_planet", false}, {"in_orbit", false}, {"reason", reason},
                         {"lat", nullptr},     {"lon", nullptr},     {"destination", nullptr}};
    };
    if (!view || !view->have_player_env) return not_ready(T("waiting"));

    const int pos_source = SettingInt("pos_source", 0);
    const bool env_add_offset = SettingBool("env_add_offset", false);
    const bool planet_local = SettingBool("planet_pos_is_local", false);
    const int convention = SettingInt("convention", 0);
    const bool invert_lat = SettingBool("invert_lat", false);
    const bool arrow_flip = SettingBool("arrow_flip", false);

    geo::Vec3 goff = view->have_player ? view->graphics_offset : geo::Vec3{};
    geo::Vec3 world, offset;
    if (pos_source == 0) {  // Entorno: cGcPlayerEnvironment.mPlayerTM (a pie, en nave o en vehiculo)
        offset = env_add_offset ? goff : geo::Vec3{};
        world = {view->tm_pos.x + offset.x, view->tm_pos.y + offset.y, view->tm_pos.z + offset.z};
    } else {  // Cuerpo: cGcPlayer.mGraphicsMatrix
        offset = goff;
        world = {view->graphics_local.x + goff.x, view->graphics_local.y + goff.y, view->graphics_local.z + goff.z};
    }
    geo::Vec3 center = view->nearest_planet_pos;
    if (planet_local) center = {center.x + offset.x, center.y + offset.y, center.z + offset.z};
    geo::Vec3 rel = geo::Sub(world, center);
    double dist_center = geo::Length(rel);
    std::string key = PlanetKey(*view);

    Json& radii = data_["radii"];
    bool in_orbit;
    double radius;
    if (IsPlanetLocation(view->location)) {
        in_orbit = false;
        radius = dist_center;
        double old = radii.contains(key) && radii[key].is_number() ? radii[key].get<double>() : 0.0;
        if (old <= 0 || std::fabs(old - dist_center) / old > 0.02) {
            radii[key] = geo::RoundTo(dist_center, 1);
            Save();
        }
    } else if (view->location == kLocDefault && radii.contains(key) && radii[key].is_number() &&
               radii[key].get<double>() > 0 && dist_center <= kOrbitFactor * radii[key].get<double>()) {
        in_orbit = true;
        radius = radii[key].get<double>();
    } else {
        return not_ready(T("not_on_planet"));
    }

    double lat = 0, lon = 0;
    if (!geo::VectorToLatLon(rel, convention, invert_lat, &lat, &lon)) return not_ready(T("not_on_planet"));

    if (key != stable_key_) {
        stable_key_ = key;
        stable_since_ = now;
    }
    bool stable = (now - stable_since_) >= kStableSeconds;

    context_ = Json{{"world", {world.x, world.y, world.z}},
                    {"offset", {offset.x, offset.y, offset.z}},
                    {"center", {center.x, center.y, center.z}},
                    {"rel", {rel.x, rel.y, rel.z}},
                    {"dist_center", dist_center},
                    {"radius", radius},
                    {"in_orbit", in_orbit},
                    {"planet_key", key},
                    {"location", view->location},
                    {"lat", lat},
                    {"lon", lon},
                    {"stable", stable}};

    Json dest_info = nullptr;
    Json& dests = data_["destinations"];
    if (dests.contains(key) && dests[key].is_object() && dests[key].contains("lat") && dests[key].contains("lon")) {
        const Json& d = dests[key];
        double dlat = d["lat"].get<double>(), dlon = d["lon"].get<double>();
        std::string name = d.contains("name") && d["name"].is_string() ? d["name"].get<std::string>() : "";
        double dist = geo::SurfaceDistance(lat, lon, dlat, dlon, radius);
        double brg = geo::InitialBearing(lat, lon, dlat, dlon);
        dest_info = Json{{"lat", d["lat"]},
                         {"lon", d["lon"]},
                         {"name", name},
                         {"distance_m", geo::RoundTo(dist, 1)},
                         {"bearing", static_cast<int>(geo::RoundTo(brg, 0))},
                         {"cardinal", geo::Cardinal(brg, LangLocked())}};
        geo::Vec3 u = geo::LatLonToUnit(dlat, dlon, convention, invert_lat);
        geo::Vec3 target{center.x + u.x * radius, center.y + u.y * radius, center.z + u.z * radius};
        double yaw = 0, pitch = 0;
        geo::RelativeDirection(world, target, view->tm_right, view->tm_up, view->tm_at, arrow_flip, &yaw, &pitch);
        dest_info["yaw"] = geo::RoundTo(yaw, 1);
        dest_info["pitch"] = geo::RoundTo(pitch, 1);
        if (!in_orbit && stable && dist <= kArrivalMeters) {
            std::string shown = name.empty() ? FormatCoords(dlat, dlon) : name;
            dests.erase(key);
            Save();
            SetResult(true, Replace(T("arrived"), "{name}", shown));
            dest_info = nullptr;
        }
    }
    snapshot_ = Json{{"ok", true},
                     {"on_planet", !in_orbit},
                     {"in_orbit", in_orbit},
                     {"reason", ""},
                     {"lat", geo::RoundTo(lat, 2)},
                     {"lon", geo::RoundTo(lon, 2)},
                     {"planet_key", key},
                     {"destination", dest_info}};
}

// ---- respuestas HTTP ------------------------------------------------------------------------
std::string Core::CoordsJson() {
    std::lock_guard<std::mutex> g(mu_);
    Json snap = snapshot_;
    snap["history"] = data_["history"];
    snap["last_result"] = last_result_;
    snap["input_request_id"] = 0;
    snap["version"] = kModVersion;
    snap["hotkey"] = "ninguna";  // la tecla llega en el paso 4 del DLL
    std::string saved = data_["settings"].value("hotkey", "f9");
    snap["hotkey_saved"] = saved;
    snap["lang"] = LangLocked();
    return snap.dump(-1, ' ', false, Json::error_handler_t::replace);
}

std::string Core::DebugJson(const Json& extra) {
    std::lock_guard<std::mutex> g(mu_);
    Json d{{"snapshot", snapshot_},
           {"settings", data_["settings"]},
           {"data_path", data_path_},
           {"destinations", data_["destinations"]},
           {"context", context_},
           {"version", kModVersion},
           {"credits",
            {{"author", "MightyZeus"},
             {"repo", "https://github.com/ImMightyZeus/Olympian-Planet-Coords"},
             {"paypal", "https://paypal.me/ImMightyZeus"},
             {"license", "GPL-3.0"}}}};
    for (auto& [k, v] : extra.items()) d[k] = v;
    return d.dump(-1, ' ', false, Json::error_handler_t::replace);
}

std::string Core::Put(const std::string& route, const Json& query) {
    std::lock_guard<std::mutex> g(mu_);
    Json r;
    if (route == "lang") {
        std::string c = query.value("code", "");
        for (auto& ch : c) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (c == "es" || c == "en") {
            data_["settings"]["lang"] = c;
            Save();
            r = Json{{"saved", true}, {"lang", c}, {"gui_lang", c}};
        } else {
            r = Json{{"saved", false}, {"message", T("bad_lang")}};
        }
    } else if (route == "hotkey" || route == "marker" || route == "clear") {
        r = Json{{"queued", false}, {"saved", false}, {"message", T("dll_not_yet")}};
    } else {
        r = Json{{"detail", "Not Found"}};
    }
    return r.dump(-1, ' ', false, Json::error_handler_t::replace);
}

}  // namespace olympian
