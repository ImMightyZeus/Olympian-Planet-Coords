// Olympian Planet Coords — matematica del planeta (port exacto de la version Python v0.0.3).
// Olympian Planet Coords — planet math (exact port of the Python v0.0.3 version).
//
// Olympian Planet Coords — Copyright © 2026 MightyZeus · GPL-3.0 License
// Sin dependencias del juego ni de Windows: se prueba en Linux contra la version Python.
#pragma once

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace geo {

constexpr double kPi = 3.14159265358979323846;
inline double Deg(double r) { return r * 180.0 / kPi; }
inline double Rad(double d) { return d * kPi / 180.0; }

struct Vec3 {
    double x = 0, y = 0, z = 0;
};

inline Vec3 Sub(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline double Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline double Length(const Vec3& v) { return std::sqrt(Dot(v, v)); }

// Igual que LonConvention de Python.
enum LonConvention { atan2_x_z = 0, atan2_z_x = 1, atan2_negx_z = 2, atan2_negz_x = 3 };

// Vector relativo al centro del planeta -> (lat, lon) en grados. lon en (-180, 180].
inline bool VectorToLatLon(const Vec3& rel, int convention, bool invert_lat, double* lat, double* lon) {
    double len = Length(rel);
    if (len < 1e-6) return false;
    double x = rel.x / len, y = rel.y / len, z = rel.z / len;
    double la = Deg(std::asin(std::fmax(-1.0, std::fmin(1.0, y))));
    if (invert_lat) la = -la;
    double lo;
    switch (convention) {
        case atan2_x_z: lo = std::atan2(x, z); break;
        case atan2_z_x: lo = std::atan2(z, x); break;
        case atan2_negx_z: lo = std::atan2(-x, z); break;
        default: lo = std::atan2(-z, x); break;
    }
    lo = Deg(lo);
    if (lo <= -180.0) lo += 360.0;
    *lat = la;
    *lon = lo;
    return true;
}

// Inversa exacta de VectorToLatLon (vector unitario).
inline Vec3 LatLonToUnit(double lat, double lon, int convention, bool invert_lat) {
    if (invert_lat) lat = -lat;
    double la = Rad(lat), lo = Rad(lon);
    double y = std::sin(la), h = std::cos(la);
    Vec3 v;
    v.y = y;
    switch (convention) {
        case atan2_x_z: v.x = h * std::sin(lo); v.z = h * std::cos(lo); break;
        case atan2_z_x: v.z = h * std::sin(lo); v.x = h * std::cos(lo); break;
        case atan2_negx_z: v.x = -h * std::sin(lo); v.z = h * std::cos(lo); break;
        default: v.z = -h * std::sin(lo); v.x = h * std::cos(lo); break;
    }
    return v;
}

// Distancia sobre la superficie (gran circulo, haversine) en las unidades de radius.
inline double SurfaceDistance(double lat1, double lon1, double lat2, double lon2, double radius) {
    double p1 = Rad(lat1), p2 = Rad(lat2);
    double dp = p2 - p1, dl = Rad(lon2 - lon1);
    double a = std::pow(std::sin(dp / 2), 2) + std::cos(p1) * std::cos(p2) * std::pow(std::sin(dl / 2), 2);
    return 2 * radius * std::asin(std::fmin(1.0, std::sqrt(a)));
}

// Rumbo inicial desde 1 hacia 2, en grados (0 = norte, 90 = este).
inline double InitialBearing(double lat1, double lon1, double lat2, double lon2) {
    double p1 = Rad(lat1), p2 = Rad(lat2), dl = Rad(lon2 - lon1);
    double x = std::sin(dl) * std::cos(p2);
    double y = std::cos(p1) * std::sin(p2) - std::sin(p1) * std::cos(p2) * std::cos(dl);
    double b = std::fmod(Deg(std::atan2(x, y)) + 360.0, 360.0);
    return b < 0 ? b + 360.0 : b;
}

// Python: names[int(((bearing % 360) + 22.5) // 45) % 8]
inline std::string Cardinal(double bearing, const std::string& lang) {
    static const char* es[8] = {"N", "NE", "E", "SE", "S", "SO", "O", "NO"};
    static const char* en[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    double b = std::fmod(bearing, 360.0);
    if (b < 0) b += 360.0;
    int i = static_cast<int>(std::floor((b + 22.5) / 45.0)) % 8;
    return (lang == "en" ? en : es)[i];
}

// Giro (yaw) e inclinacion (pitch) en grados del objetivo visto desde origin con la base
// (right, up, at). yaw 0 = de frente, +90 = derecha, +-180 = detras; pitch + = arriba.
inline void RelativeDirection(const Vec3& origin, const Vec3& target, const Vec3& right, const Vec3& up,
                              const Vec3& at, bool flip_forward, double* yaw, double* pitch) {
    Vec3 d = Sub(target, origin);
    double s = flip_forward ? -1.0 : 1.0;
    double fwd = Dot(d, at) * s, side = Dot(d, right) * s, vert = Dot(d, up);
    *yaw = Deg(std::atan2(side, fwd));
    *pitch = Deg(std::atan2(vert, std::hypot(side, fwd)));
}

// Redondeo como round() de Python: redondea el valor binario exacto (2.675 -> 2.67), igual que
// printf con redondeo correcto. digits entre 0 y 6.
inline double RoundTo(double v, int digits) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", digits, v);
    return std::strtod(buf, nullptr);
}

}  // namespace geo
