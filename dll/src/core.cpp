// Olympian Planet Coords — nucleo portable del DLL. Ver core.hpp.
// Olympian Planet Coords — Copyright © 2026 MightyZeus · GPL-3.0 License

#include "core.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
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
    {"dest_set", {"Destino: {coords}", "Destination: {coords}"}},
    {"dest_cleared", {"Destino quitado", "Destination cleared"}},
    {"no_dest_here", {"no hay destino en este planeta", "no destination on this planet"}},
    {"out_of_range", {"coordenadas fuera de rango", "coordinates out of range"}},
    {"bad_hotkey", {"tecla no valida", "invalid key"}},
    {"marker_label", {"Destino", "Destination"}},
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

std::string Trim(const std::string& s) {
    const char* ws = " \t\r\n\f\v";
    size_t a = s.find_first_not_of(ws);
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(ws);
    return s.substr(a, b - a + 1);
}

// Numero de la consulta HTTP. Acepta "12.5" y "12,5"; false si no es un numero finito.
bool ParseNumber(const Json& q, const char* key, double* out) {
    if (!q.contains(key)) return false;
    std::string t = q[key].is_string() ? Trim(q[key].get<std::string>()) : q[key].dump();
    for (auto& c : t)
        if (c == ',') c = '.';
    if (t.empty()) return false;
    char* end = nullptr;
    double v = std::strtod(t.c_str(), &end);
    if (!end || *end != '\0' || !std::isfinite(v)) return false;
    *out = v;
    return true;
}

std::string Lower(std::string s) {
    for (auto& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

// Igual que push_history de Python: el mas reciente primero, sin repetir lat/lon, maximo 10.
Json PushHistory(const Json& history, double lat, double lon, const std::string& name) {
    Json entry{{"lat", geo::RoundTo(lat, 2)}, {"lon", geo::RoundTo(lon, 2)}, {"name", name}};
    Json out = Json::array({entry});
    if (history.is_array())
        for (const auto& h : history) {
            if (out.size() >= static_cast<size_t>(kHistoryMax)) break;
            bool same = h.is_object() && h.contains("lat") && h.contains("lon") && h["lat"].is_number() &&
                        h["lon"].is_number() && h["lat"].get<double>() == entry["lat"].get<double>() &&
                        h["lon"].get<double>() == entry["lon"].get<double>();
            if (!same) out.push_back(h);
        }
    return out;
}

}  // namespace

const char* const kHotkeyChoices[8] = {"f5", "f6", "f7", "f8", "f9", "f10", "f11", "ninguna"};

int HotkeyVirtualKey(const std::string& name) {
    // VK_F5 = 0x74 ... VK_F11 = 0x7A
    for (int i = 0; i < 7; ++i)
        if (name == kHotkeyChoices[i]) return 0x74 + i;
    return 0;
}

std::string Utf8TruncateBytes(const std::string& s, size_t max_bytes) {
    if (s.size() <= max_bytes) return s;
    size_t n = max_bytes;
    while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80) --n;  // no partir un caracter
    return s.substr(0, n);
}

std::string Utf8TruncateChars(const std::string& s, size_t max_chars) {
    size_t chars = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) {
            if (chars == max_chars) return s.substr(0, i);
            ++chars;
        }
    }
    return s;
}

std::string Tr(const std::string& key, const std::string& lang) {
    auto it = kMessages.find(key);
    if (it == kMessages.end()) return key;
    return lang == "en" ? it->second.second : it->second.first;
}

Core::Core(std::string data_path, std::string system_lang)
    : data_path_(std::move(data_path)), system_lang_(system_lang == "en" ? "en" : "es") {
    last_result_ = Json{{"ok", nullptr}, {"message", ""}, {"seq", 0}};
    Load();
    UpdateHotkeyLocked();
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

// Tecla guardada, normalizada como normalize_hotkey de Python (desconocida -> f9).
static std::string SavedHotkey(const Json& settings) {
    std::string k = settings.contains("hotkey") && settings["hotkey"].is_string()
                        ? Lower(Trim(settings["hotkey"].get<std::string>()))
                        : "";
    for (const char* c : kHotkeyChoices)
        if (k == c) return k;
    return "f9";
}

void Core::UpdateHotkeyLocked() { hotkey_vk_.store(HotkeyVirtualKey(SavedHotkey(data_["settings"]))); }

void Core::SetMarkerLocked(const MarkerSpec& m) {
    std::lock_guard<std::mutex> g(marker_mu_);
    marker_ = m;
}

MarkerSpec Core::Marker() {
    std::lock_guard<std::mutex> g(marker_mu_);
    return marker_;
}

void Core::NoteMarkerCall(bool drawn) { ++(drawn ? marker_drawn_ : marker_skipped_); }

void Core::KeyState(bool down, bool focused) {
    bool press = down && !key_was_down_;
    key_was_down_ = down;
    if (press && focused) {
        std::lock_guard<std::mutex> g(mu_);
        ++input_request_id_;
    }
}

// ---- un frame del juego (port de _read_context + _update) --------------------------------------
void Core::Tick(const GameView* view, double now) {
    std::lock_guard<std::mutex> g(mu_);
    auto not_ready = [&](const std::string& reason) {
        SetMarkerLocked(MarkerSpec{});
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
    // Marcador en el HUD: como _sync_marker + on_base_building_update de Python (metodo
    // BaseBuildingHUD): solo con destino en este planeta y 1 s despues de llegar a el.
    MarkerSpec m;
    if (stable && dests.contains(key) && dests[key].is_object() && dests[key].contains("lat") &&
        dests[key].contains("lon")) {
        const Json& d = dests[key];
        double dlat = d["lat"].get<double>(), dlon = d["lon"].get<double>();
        std::string name = d.contains("name") && d["name"].is_string() ? d["name"].get<std::string>() : "";
        geo::Vec3 u = geo::LatLonToUnit(dlat, dlon, convention, invert_lat);
        geo::Vec3 tw{center.x + u.x * radius, center.y + u.y * radius, center.z + u.z * radius};
        m.valid = true;
        m.local[0] = static_cast<float>(tw.x - offset.x);
        m.local[1] = static_cast<float>(tw.y - offset.y);
        m.local[2] = static_cast<float>(tw.z - offset.z);
        m.offset[0] = static_cast<float>(offset.x);
        m.offset[1] = static_cast<float>(offset.y);
        m.offset[2] = static_cast<float>(offset.z);
        std::string label = name;
        if (label.empty()) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), " %+.2f,%+.2f", dlat, dlon);
            label = T("marker_label") + buf;
        }
        label = Utf8TruncateBytes(label, sizeof(m.label) - 1);
        std::memcpy(m.label, label.data(), label.size());
        m.label[label.size()] = '\0';
    }
    SetMarkerLocked(m);
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
    snap["input_request_id"] = input_request_id_;
    snap["version"] = kModVersion;
    std::string saved = SavedHotkey(data_["settings"]);
    snap["hotkey"] = saved;  // en el DLL la tecla elegida se activa al momento
    snap["hotkey_saved"] = saved;
    snap["lang"] = LangLocked();
    return snap.dump(-1, ' ', false, Json::error_handler_t::replace);
}

std::string Core::DebugJson(const Json& extra) {
    bool marker_valid = Marker().valid;
    std::lock_guard<std::mutex> g(mu_);
    Json d{{"snapshot", snapshot_},
           {"settings", data_["settings"]},
           {"data_path", data_path_},
           {"destinations", data_["destinations"]},
           {"context", context_},
           {"version", kModVersion},
           {"active_marker", marker_valid},
           {"marker_frames_drawn", marker_drawn_.load()},
           {"marker_frames_skipped", marker_skipped_.load()},
           {"hotkey", SavedHotkey(data_["settings"])},
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
    } else if (route == "hotkey") {
        std::string k = Lower(Trim(query.value("key", "")));
        bool ok = false;
        for (const char* c : kHotkeyChoices) ok = ok || k == c;
        if (!ok) {
            r = Json{{"saved", false}, {"message", T("bad_hotkey")}};
        } else {
            data_["settings"]["hotkey"] = k;
            Save();
            UpdateHotkeyLocked();
            r = Json{{"saved", true}, {"hotkey", k}, {"hotkey_saved", k}, {"pending_reload", false}};
        }
    } else if (route == "marker") {
        // Igual que http_marker + _process_requests("place") de Python.
        double lat = 0, lon = 0;
        if (!ParseNumber(query, "lat", &lat) || !ParseNumber(query, "lon", &lon) || lat < -90 || lat > 90 ||
            lon < -180 || lon > 180) {
            r = Json{{"queued", false}, {"message", T("out_of_range")}};
        } else {
            std::string name = Utf8TruncateChars(Trim(query.value("name", "")), kNameMaxChars);
            r = Json{{"queued", true}};
            if (context_.is_null()) {
                std::string reason = snapshot_.value("reason", "");
                SetResult(false, reason.empty() ? T("not_on_planet") : reason);
            } else {
                std::string key = context_["planet_key"].get<std::string>();
                data_["destinations"][key] =
                    Json{{"lat", geo::RoundTo(lat, 2)}, {"lon", geo::RoundTo(lon, 2)}, {"name", name}};
                data_["history"] = PushHistory(data_["history"], lat, lon, name);
                Save();
                std::string coords = FormatCoords(lat, lon) + (name.empty() ? "" : " " + name);
                SetResult(true, Replace(T("dest_set"), "{coords}", coords));
            }
        }
    } else if (route == "clear") {
        r = Json{{"queued", true}};
        std::string key = context_.is_null() ? "" : context_["planet_key"].get<std::string>();
        if (key.empty() || !data_["destinations"].contains(key)) {
            SetResult(false, T("no_dest_here"));
        } else {
            data_["destinations"].erase(key);
            Save();
            SetResult(true, T("dest_cleared"));
        }
    } else {
        r = Json{{"detail", "Not Found"}};
    }
    return r.dump(-1, ' ', false, Json::error_handler_t::replace);
}

}  // namespace olympian
