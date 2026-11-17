// Olympian Planet Coords — servidor HTTP minimo. Ver http_server.hpp.
// Olympian Planet Coords — Copyright © 2026 MightyZeus · GPL-3.0 License

#include "http_server.hpp"

#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using SocketT = SOCKET;
static void CloseSock(SocketT s) { closesocket(s); }
static constexpr SocketT kBad = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketT = int;
static void CloseSock(SocketT s) { close(s); }
static constexpr SocketT kBad = -1;
#endif

namespace olympian {

namespace {

int HexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

const char* StatusText(int s) {
    switch (s) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        default: return "Internal Server Error";
    }
}

void SendAll(SocketT s, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        int n = send(s, data.data() + sent, static_cast<int>(data.size() - sent), 0);
        if (n <= 0) return;
        sent += static_cast<size_t>(n);
    }
}

}  // namespace

std::string UrlDecode(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') {
            out += ' ';
        } else if (s[i] == '%' && i + 2 < s.size() && HexVal(s[i + 1]) >= 0 && HexVal(s[i + 2]) >= 0) {
            out += static_cast<char>(HexVal(s[i + 1]) * 16 + HexVal(s[i + 2]));
            i += 2;
        } else {
            out += s[i];
        }
    }
    return out;
}

bool ParseRequestLine(const std::string& head, HttpRequest* req) {
    size_t eol = head.find("\r\n");
    std::string line = head.substr(0, eol);
    size_t a = line.find(' ');
    if (a == std::string::npos) return false;
    size_t b = line.find(' ', a + 1);
    if (b == std::string::npos) return false;
    req->method = line.substr(0, a);
    std::string target = line.substr(a + 1, b - a - 1);
    size_t q = target.find('?');
    req->path = UrlDecode(target.substr(0, q));
    req->query.clear();
    if (q != std::string::npos) {
        std::string qs = target.substr(q + 1);
        size_t pos = 0;
        while (pos <= qs.size()) {
            size_t amp = qs.find('&', pos);
            std::string pair = qs.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);
            if (!pair.empty()) {
                size_t eq = pair.find('=');
                std::string k = UrlDecode(pair.substr(0, eq));
                std::string v = eq == std::string::npos ? "" : UrlDecode(pair.substr(eq + 1));
                req->query[k] = v;
            }
            if (amp == std::string::npos) break;
            pos = amp + 1;
        }
    }
    return !req->method.empty() && !req->path.empty() && req->path[0] == '/';
}

HttpServer::~HttpServer() { Stop(); }

bool HttpServer::Start(int port, Handler handler, std::string* error) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        if (error) *error = "WSAStartup";
        return false;
    }
#endif
    SocketT s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == kBad) {
        if (error) *error = "socket";
        return false;
    }
#ifdef _WIN32
    BOOL excl = TRUE;  // nadie mas puede "robar" el puerto mientras lo usamos
    setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&excl), sizeof(excl));
#else
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
#endif
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<unsigned short>(port));
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // solo este PC
    if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 || listen(s, 16) != 0) {
        if (error) *error = "puerto " + std::to_string(port) + " ocupado";
        CloseSock(s);
        return false;
    }
    listen_socket_ = static_cast<long long>(s);
    handler_ = std::move(handler);
    running_ = true;
    thread_ = std::thread([this] { Loop(); });
    return true;
}

void HttpServer::Stop() {
    if (!running_.exchange(false)) return;
    if (listen_socket_ != -1) CloseSock(static_cast<SocketT>(listen_socket_));
    listen_socket_ = -1;
    if (thread_.joinable()) thread_.join();
}

void HttpServer::Loop() {
    SocketT ls = static_cast<SocketT>(listen_socket_);
    while (running_) {
        SocketT c = accept(ls, nullptr, nullptr);
        if (c == kBad) {
            if (!running_) break;
            continue;
        }
#ifdef _WIN32
        DWORD tv = 2000;
        setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
        timeval tv{2, 0};
        setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
        std::string head;
        char buf[2048];
        while (head.find("\r\n\r\n") == std::string::npos && head.size() < 16384) {
            int n = recv(c, buf, sizeof(buf), 0);
            if (n <= 0) break;
            head.append(buf, static_cast<size_t>(n));
        }
        HttpResponse resp;
        HttpRequest req;
        if (!ParseRequestLine(head, &req)) {
            resp = {400, "{\"detail\":\"Bad Request\"}"};
        } else {
            try {
                resp = handler_(req);
            } catch (...) {
                resp = {500, "{\"detail\":\"error\"}"};
            }
        }
        std::string out = "HTTP/1.1 " + std::to_string(resp.status) + " " + StatusText(resp.status) +
                          "\r\nContent-Type: application/json; charset=utf-8\r\nContent-Length: " +
                          std::to_string(resp.body.size()) + "\r\nConnection: close\r\n\r\n" + resp.body;
        SendAll(c, out);
        CloseSock(c);
    }
}

}  // namespace olympian
