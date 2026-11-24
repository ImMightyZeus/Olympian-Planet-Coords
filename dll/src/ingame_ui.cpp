// Olympian Planet Coords — panel dentro del juego (ver ingame_ui.hpp).
// Olympian Planet Coords — in-game panel (see ingame_ui.hpp).
//
// Olympian Planet Coords — Copyright © 2026 MightyZeus · GPL-3.0 License
#include "ingame_ui.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <regex>

namespace olympian {

namespace {

// Codigos de estilo de linea (ver vk_overlay.hpp).
const std::string kTitle = "\x10", kBig = "\x11", kSmall = "\x12", kItem = "\x13", kSel = "\x14", kErr = "\x15",
                  kLabel = "\x16", kInput = "\x17", kOk = "\x18", kNum = "\x01";

struct UiMsg {
    const char* es;
    const char* en;
    const char* ru;
};

// Textos del panel (solo del DLL).
const std::map<std::string, UiMsg> kUi = {
    {"title_dest", {"DESTINO", "DESTINATION", "ЦЕЛЬ"}},
    {"title_settings", {"AJUSTES", "SETTINGS", "НАСТРОЙКИ"}},
    {"prompt", {"Escribe latitud, longitud y un nombre (opcional):", "Type latitude, longitude and a name (optional):",
                "Введите широту, долготу и название (необязательно):"}},
    {"saved", {"Destinos guardados", "Saved destinations", "Сохранённые цели"}},
    {"no_saved", {"Todavia no hay destinos guardados", "No saved destinations yet", "Сохранённых целей пока нет"}},
    {"go", {"Enter: ir", "Enter: go", "Enter: идти"}},
    {"del", {"Supr: quitar", "Del: remove", "Del: удалить"}},
    {"hint_dest",
     {"Enter: fijar · \xE2\x86\x91\xE2\x86\x93: lista · Tab: ajustes · Esc: cerrar",
      "Enter: set · \xE2\x86\x91\xE2\x86\x93: list · Tab: settings · Esc: close",
      "Enter: задать · \xE2\x86\x91\xE2\x86\x93: список · Tab: настройки · Esc: закрыть"}},
    {"hint_settings",
     {"\xE2\x86\x91\xE2\x86\x93 elegir \xC2\xB7 \xE2\x86\x90\xE2\x86\x92 cambiar \xC2\xB7 Tab: destinos \xC2\xB7 Esc: cerrar",
      "\xE2\x86\x91\xE2\x86\x93 choose \xC2\xB7 \xE2\x86\x90\xE2\x86\x92 change \xC2\xB7 Tab: destinations \xC2\xB7 Esc: close",
      "\xE2\x86\x91\xE2\x86\x93 выбор \xC2\xB7 \xE2\x86\x90\xE2\x86\x92 изменить \xC2\xB7 Tab: цели \xC2\xB7 Esc: закрыть"}},
    {"lang", {"Idioma", "Language", "Язык"}},
    {"key", {"Tecla del panel", "Panel key", "Клавиша панели"}},
    {"clear", {"Quitar el destino de este planeta", "Clear this planet's destination", "Удалить цель на этой планете"}},
    {"parse_format",
     {"formato: usa  +12.34, -56.78  (o 12,34; -56,78)", "format: use  +12.34, -56.78  (or 12,34; -56,78)",
      "формат:  +12.34, -56.78  (или 12,34; -56,78)"}},
    {"parse_lat",
     {"latitud fuera de rango (-90..90)", "latitude out of range (-90..90)", "широта вне диапазона (-90..90)"}},
    {"parse_lon",
     {"longitud fuera de rango (-180..180)", "longitude out of range (-180..180)",
      "долгота вне диапазона (-180..180)"}},
    {"empty", {"escribe unas coordenadas o elige uno de la lista", "type coordinates or pick one from the list",
               "введите координаты или выберите из списка"}},
    {"lang_saved", {"Idioma: Espa\xC3\xB1ol", "Language: English", "Язык: Русский"}},
    {"key_saved", {"Tecla del panel: {key}", "Panel key: {key}", "Клавиша панели: {key}"}},
};

const char* const kLangs[3] = {"es", "en", "ru"};
const char* const kLangNames[3] = {"Espa\xC3\xB1ol", "English", "Русский"};
// Teclas para abrir el panel. "ninguna" no se ofrece aqui: sin tecla no se podria volver a abrir.
const char* const kKeys[7] = {"f5", "f6", "f7", "f8", "f9", "f10", "f11"};

std::string Utf8(const std::u32string& s) {
    std::string o;
    for (char32_t c : s) {
        if (c < 0x80) {
            o += static_cast<char>(c);
        } else if (c < 0x800) {
            o += static_cast<char>(0xC0 | (c >> 6));
            o += static_cast<char>(0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            o += static_cast<char>(0xE0 | (c >> 12));
            o += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            o += static_cast<char>(0x80 | (c & 0x3F));
        } else {
            o += static_cast<char>(0xF0 | (c >> 18));
            o += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
            o += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            o += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    return o;
}

std::string Upper(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

std::string Fmt(double lat, double lon) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%+.2f, %+.2f", lat, lon);
    return buf;
}

// Espacios al principio y al final (como str.strip() de Python para texto ASCII).
std::string Strip(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n\f\v"), b = s.find_last_not_of(" \t\r\n\f\v");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

int IndexOf(const char* const* list, int n, const std::string& v) {
    for (int i = 0; i < n; ++i)
        if (v == list[i]) return i;
    return -1;
}

}  // namespace

// Mismas expresiones que _COORD_DOT_RE y _COORD_SEMI_RE de Python.
ParsedCoords ParseCoords(const std::string& text) {
    static const std::regex dot(R"(^([+-]?\d+(?:\.\d+)?)\s*[,\s]\s*([+-]?\d+(?:\.\d+)?)(?:\s*[,\s]\s*([\s\S]*))?$)");
    static const std::regex semi(
        R"(^([+-]?\d+(?:[.,]\d+)?)\s*;\s*([+-]?\d+(?:[.,]\d+)?)(?:\s*[;\s]\s*([\s\S]*))?$)");
    ParsedCoords r;
    // Digitos de ancho completo (teclados japones, chino...): como los normales, igual que \d de Python.
    std::string t;
    for (size_t i = 0; i < text.size(); ++i) {
        unsigned char c0 = static_cast<unsigned char>(text[i]);
        if (c0 == 0xEF && i + 2 < text.size() && static_cast<unsigned char>(text[i + 1]) == 0xBC) {
            unsigned char c2 = static_cast<unsigned char>(text[i + 2]);
            if (c2 >= 0x90 && c2 <= 0x99) {
                t += static_cast<char>('0' + (c2 - 0x90));
                i += 2;
                continue;
            }
        }
        t += text[i];
    }
    t = Strip(t);
    std::smatch m;
    if (!std::regex_match(t, m, t.find(';') != std::string::npos ? semi : dot)) {
        r.error = "parse_format";
        return r;
    }
    auto num = [](std::string s) {
        for (auto& c : s)
            if (c == ',') c = '.';
        return std::strtod(s.c_str(), nullptr);
    };
    r.lat = num(m[1].str());
    r.lon = num(m[2].str());
    r.name = Strip(m[3].matched ? m[3].str() : "");
    if (!(r.lat >= -90.0 && r.lat <= 90.0)) {
        r.error = "parse_lat";
        return r;
    }
    if (!(r.lon >= -180.0 && r.lon <= 180.0)) {
        r.error = "parse_lon";
        return r;
    }
    r.ok = true;
    return r;
}

Panel::Panel(PanelHost host) : host_(std::move(host)) {}

std::string Panel::T(const char* key) const {
    auto it = kUi.find(key);
    if (it == kUi.end()) return key;
    return lang_ == "ru" ? it->second.ru : lang_ == "en" ? it->second.en : it->second.es;
}

Json Panel::Snap() {
    Json s = host_.snapshot ? Json::parse(host_.snapshot(), nullptr, false) : Json();
    if (s.is_discarded() || !s.is_object()) s = Json::object();
    std::string l = s.value("lang", "es");
    lang_ = IndexOf(kLangs, 3, l) >= 0 ? l : "en";
    return s;
}

bool Panel::IsOpen() {
    std::lock_guard<std::mutex> g(mu_);
    return open_;
}

void Panel::Open() {
    std::lock_guard<std::mutex> g(mu_);
    open_ = true;
    page_ = kDest;
    input_.clear();
    sel_ = -1;
    status_.clear();
    Json s = Snap();
    last_seq_ = s.contains("last_result") ? s["last_result"].value("seq", 0) : 0;
}

void Panel::Close() {
    std::lock_guard<std::mutex> g(mu_);
    open_ = false;
}

void Panel::Toggle() {
    if (IsOpen())
        Close();
    else
        Open();
}

std::string Panel::InputUtf8() {
    std::lock_guard<std::mutex> g(mu_);
    return Utf8(input_);
}
int Panel::Selected() {
    std::lock_guard<std::mutex> g(mu_);
    return sel_;
}
int Panel::Page() {
    std::lock_guard<std::mutex> g(mu_);
    return page_;
}

void Panel::Status(bool ok, const std::string& msg) {
    status_ok_ = ok;
    status_ = msg;
}

// Respuesta de Core::Put y resultado que dejo en last_result.
void Panel::ApplyResult(const std::string& reply) {
    Status(true, "");
    Json r = Json::parse(reply, nullptr, false);
    if (!r.is_discarded() && r.is_object() && r.contains("message") && r["message"].is_string() &&
        !r.value("saved", true)) {
        Status(false, r["message"].get<std::string>());
        return;
    }
    if (!r.is_discarded() && r.is_object() && r.contains("queued") && !r.value("queued", true)) {
        Status(false, r.value("message", std::string()));
        return;
    }
    Json s = Snap();
    if (s.contains("last_result") && s["last_result"].is_object()) {
        const Json& lr = s["last_result"];
        int seq = lr.value("seq", 0);
        if (seq != last_seq_) {
            last_seq_ = seq;
            bool ok = lr.contains("ok") && lr["ok"].is_boolean() && lr["ok"].get<bool>();
            Status(ok, lr.value("message", std::string()));
        }
    }
}

void Panel::Submit() {
    std::string text = Utf8(input_);
    if (Strip(text).empty()) {
        Status(false, T("empty"));
        return;
    }
    ParsedCoords p = ParseCoords(text);
    if (!p.ok) {
        Status(false, T(p.error.c_str()));
        return;
    }
    ApplyResult(host_.put("marker", Json{{"lat", p.lat}, {"lon", p.lon}, {"name", p.name}}));
    if (status_ok_) {
        open_ = false;  // destino puesto: el panel se cierra y el juego recupera el teclado
    }
}

void Panel::UseHistory(int index) {
    Json s = Snap();
    if (!s.contains("history") || !s["history"].is_array() || index < 0 ||
        index >= static_cast<int>(s["history"].size()))
        return;
    const Json& h = s["history"][static_cast<size_t>(index)];
    ApplyResult(host_.put("marker", Json{{"lat", h.value("lat", 0.0)}, {"lon", h.value("lon", 0.0)},
                                         {"name", h.value("name", std::string())}}));
    if (status_ok_) open_ = false;
}

void Panel::ForgetHistory(int index) {
    Json s = Snap();
    if (!s.contains("history") || !s["history"].is_array() || index < 0 ||
        index >= static_cast<int>(s["history"].size()))
        return;
    const Json& h = s["history"][static_cast<size_t>(index)];
    ApplyResult(host_.put("forget", Json{{"lat", h.value("lat", 0.0)}, {"lon", h.value("lon", 0.0)}}));
    int n = static_cast<int>(Snap()["history"].size());
    if (sel_ >= n) sel_ = n - 1;  // -1 si la lista quedo vacia: vuelve a la casilla
}

void Panel::ChangeSetting(int dir) {
    Json s = Snap();
    if (setting_ == kRowLang) {
        int i = IndexOf(kLangs, 3, lang_);
        i = ((i < 0 ? 0 : i) + dir + 3) % 3;
        ApplyResult(host_.put("lang", Json{{"code", kLangs[i]}}));
        Snap();
        if (status_ok_) Status(true, T("lang_saved"));
    } else if (setting_ == kRowKey) {
        int i = IndexOf(kKeys, 7, s.value("hotkey", std::string("f9")));
        i = ((i < 0 ? 4 : i) + dir + 7) % 7;
        ApplyResult(host_.put("hotkey", Json{{"key", kKeys[i]}}));
        if (status_ok_) {
            std::string m = T("key_saved");
            m.replace(m.find("{key}"), 5, Upper(kKeys[i]));
            Status(true, m);
        }
    }
}

void Panel::OnKey(const UiEvent& e) {
    std::lock_guard<std::mutex> g(mu_);
    if (!open_) return;
    if (e.key == UiKey::Esc) {
        open_ = false;
        return;
    }
    if (e.key == UiKey::Tab) {
        page_ = page_ == kDest ? kSettings : kDest;
        status_.clear();
        return;
    }
    Json s = Snap();
    int n = s.contains("history") && s["history"].is_array() ? static_cast<int>(s["history"].size()) : 0;
    if (page_ == kSettings) {
        switch (e.key) {
            case UiKey::Up: setting_ = (setting_ + kRowCount - 1) % kRowCount; break;
            case UiKey::Down: setting_ = (setting_ + 1) % kRowCount; break;
            case UiKey::Left: ChangeSetting(-1); break;
            case UiKey::Right: ChangeSetting(+1); break;
            case UiKey::Enter:
                if (setting_ == kRowClear)
                    ApplyResult(host_.put("clear", Json::object()));
                else
                    ChangeSetting(+1);
                break;
            default: break;
        }
        return;
    }
    switch (e.key) {
        case UiKey::Char:
            if (e.ch >= 0x20 && e.ch != 0x7F && input_.size() < kPanelMaxInput) {
                input_ += e.ch;
                sel_ = -1;
                status_.clear();
            }
            break;
        case UiKey::Back:
            if (sel_ < 0 && !input_.empty()) input_.pop_back();
            sel_ = -1;
            status_.clear();
            break;
        case UiKey::Up: sel_ = n == 0 ? -1 : (sel_ <= -1 ? n - 1 : sel_ - 1); break;
        case UiKey::Down: sel_ = n == 0 || sel_ >= n - 1 ? -1 : sel_ + 1; break;
        case UiKey::Enter:
            if (sel_ >= 0)
                UseHistory(sel_);
            else
                Submit();
            break;
        case UiKey::Del:
            if (sel_ >= 0) ForgetHistory(sel_);
            break;
        default: break;
    }
}

std::string Panel::Text() {
    std::lock_guard<std::mutex> g(mu_);
    if (!open_) return "";
    Json s = Snap();
    std::string out = kTitle + "OLYMPIAN PLANET COORDS \xC2\xB7 " + T(page_ == kDest ? "title_dest" : "title_settings");
    auto line = [&out](const std::string& l) { out += "\n" + l; };
    // Donde estas (siempre a la vista)
    if (s.contains("lat") && s["lat"].is_number() && s.contains("lon") && s["lon"].is_number()) {
        line(kBig + kNum + Fmt(s["lat"].get<double>(), s["lon"].get<double>()));
    } else if (s.contains("place") && s["place"].is_object() && !s["place"].value("title", std::string()).empty()) {
        line(kBig + s["place"].value("title", std::string()));
    } else {
        line(kBig + s.value("reason", std::string()));
    }
    if (page_ == kDest) {
        line(kSmall + T("prompt"));
        line(kInput + "> " + Utf8(input_) + (sel_ < 0 ? "_" : ""));
        int n = s.contains("history") && s["history"].is_array() ? static_cast<int>(s["history"].size()) : 0;
        line(kLabel + (n ? T("saved") : T("no_saved")));
        for (int i = 0; i < n; ++i) {
            const Json& h = s["history"][static_cast<size_t>(i)];
            std::string name = h.value("name", std::string());
            std::string row = Fmt(h.value("lat", 0.0), h.value("lon", 0.0)) + (name.empty() ? "" : "  " + name);
            if (i == sel_)
                line(kSel + "\xC2\xBB " + row + "   [" + T("go") + "]  [" + T("del") + "]");
            else
                line(kItem + "  " + row);
        }
        line(kSmall + T("hint_dest"));
    } else {
        std::string lang_name = kLangNames[std::max(0, IndexOf(kLangs, 3, lang_))];
        std::string key = Upper(s.value("hotkey", std::string("f9")));
        const std::string rows[kRowCount] = {T("lang") + ": \xE2\x80\xB9 " + lang_name + " \xE2\x80\xBA",
                                             T("key") + ": \xE2\x80\xB9 " + key + " \xE2\x80\xBA", T("clear")};
        for (int i = 0; i < kRowCount; ++i) line((i == setting_ ? kSel + "\xC2\xBB " : kItem + "  ") + rows[i]);
        line(kSmall + T("hint_settings"));
    }
    if (!status_.empty()) line((status_ok_ ? kOk : kErr) + status_);
    return out;
}

}  // namespace olympian
