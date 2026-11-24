// Olympian Planet Coords — panel dentro del juego: escribir destinos, lista de destinos y ajustes, todo
// con el teclado y sin el recuadro .exe.
// Olympian Planet Coords — in-game panel: type destinations, destination list and settings, all with
// the keyboard and without the .exe box.
//
// Olympian Planet Coords — Copyright © 2026 MightyZeus · GPL-3.0 License
//
// Codigo portable (sin Windows): las pruebas lo compilan en Linux. El panel no toca el juego: lee la
// foto de Core (CoordsJson) y pide cambios con Core::Put, igual que hacia el recuadro .exe por HTTP.
#pragma once

#include <functional>
#include <mutex>
#include <string>

#include "core.hpp"  // Json (nlohmann::ordered_json), sin Windows

namespace olympian {

// Teclas que entiende el panel. Char lleva la letra ya traducida con la distribucion del teclado.
enum class UiKey { Char, Back, Enter, Esc, Up, Down, Left, Right, Del, Tab };

struct UiEvent {
    UiKey key = UiKey::Char;
    char32_t ch = 0;
};

// Lo que el panel necesita de Core.
struct PanelHost {
    std::function<std::string()> snapshot;                                 // = Core::CoordsJson
    std::function<std::string(const std::string&, const Json&)> put;      // = Core::Put
};

// Lectura de coordenadas escritas: igual que parse_coords() del recuadro .exe y de la version Python
// ("+12.34, -56.78 Nombre" o "12,34; -56,78 Nombre"). error: "parse_format", "parse_lat" o "parse_lon".
struct ParsedCoords {
    bool ok = false;
    double lat = 0, lon = 0;
    std::string name;
    std::string error;
};
ParsedCoords ParseCoords(const std::string& text_utf8);

// Texto del panel para el recuadro (formato de lineas en vk_overlay.hpp).
constexpr size_t kPanelMaxInput = 80;  // letras

class Panel {
   public:
    explicit Panel(PanelHost host);

    bool IsOpen();
    void Open();
    void Close();
    void Toggle();
    // Una tecla mientras el panel esta abierto (desde el hilo del juego).
    void OnKey(const UiEvent& e);
    // Lineas del recuadro (UTF-8, una por linea, con los codigos de estilo de vk_overlay.hpp).
    // Vacio si esta cerrado.
    std::string Text();

    // Para las pruebas.
    std::string InputUtf8();
    int Selected();
    int Page();

   private:
    enum PageId { kDest = 0, kSettings = 1 };
    enum SettingRow { kRowLang = 0, kRowKey = 1, kRowClear = 2, kRowCount = 3 };

    std::string T(const char* key) const;
    Json Snap();
    void Status(bool ok, const std::string& msg);
    void ApplyResult(const std::string& put_reply);
    void Submit();
    void UseHistory(int index);
    void ForgetHistory(int index);
    void ChangeSetting(int dir);

    PanelHost host_;
    std::mutex mu_;
    bool open_ = false;
    int page_ = kDest;
    std::u32string input_;
    int sel_ = -1;          // destinos: -1 = casilla de texto; 0.. = fila de la lista
    int setting_ = kRowLang;
    std::string lang_ = "es";
    std::string status_;
    bool status_ok_ = true;
    int last_seq_ = -1;
};

}  // namespace olympian
