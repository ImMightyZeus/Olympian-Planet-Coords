// Olympian Planet Coords — servidor HTTP minimo (solo 127.0.0.1) para el overlay.
// Olympian Planet Coords — minimal HTTP server (127.0.0.1 only) for the overlay.
//
// Olympian Planet Coords — Copyright © 2026 MightyZeus · GPL-3.0 License
//
// Responde las mismas rutas que NMS.py servia para la version Python (puerto 5000), asi el overlay
// funciona igual con el DLL. Solo escucha en 127.0.0.1: nada fuera de este PC puede conectarse.
#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <string>
#include <thread>

namespace olympian {

struct HttpRequest {
    std::string method;                         // "GET", "PUT"...
    std::string path;                           // sin la consulta, p. ej. "/OlympianPlanetCoords/coords"
    std::map<std::string, std::string> query;   // parametros ya decodificados (UTF-8)
};

struct HttpResponse {
    int status = 200;
    std::string body;  // JSON
};

std::string UrlDecode(const std::string& s);
bool ParseRequestLine(const std::string& head, HttpRequest* req);

class HttpServer {
   public:
    using Handler = std::function<HttpResponse(const HttpRequest&)>;
    ~HttpServer();
    // Devuelve false si no se pudo abrir el puerto (por ejemplo, NMS.py ya lo esta usando).
    bool Start(int port, Handler handler, std::string* error);
    void Stop();

   private:
    void Loop();
    std::atomic<bool> running_{false};
    std::thread thread_;
    Handler handler_;
    long long listen_socket_ = -1;
};

}  // namespace olympian
