// Olympian Planet Coords — flecha 3D hacia el destino (geometria), igual que arrow_geometry() del
// recuadro .exe (overlay/olympian_overlay.py). Codigo portable: las pruebas lo comparan con Python.
// Olympian Planet Coords — 3D arrow towards the destination (geometry), same as the .exe box.
//
// Olympian Planet Coords — Copyright © 2026 MightyZeus · GPL-3.0 License
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace olympian {

// Unidades: caja de 80x80 centrada en (0, 0).
struct ArrowFace {
    double z;
    std::array<double, 6> pts;  // x0,y0,x1,y1,x2,y2
    unsigned char r, g, b;
};
struct ArrowSphere {
    double cx, cy, r;
    unsigned char red, green, blue;
};
struct ArrowGeo {
    std::vector<ArrowFace> faces;  // de la mas lejana a la mas cercana
    double ring_r;
    std::vector<ArrowSphere> sphere;
};

// yaw: 0 = destino de frente (flecha hacia arriba), +90 = a la derecha, 180 = detras.
// pitch: + = destino por encima. morph: 0 = flecha completa; 1 = esfera en reposo y circulo al 40%.
inline ArrowGeo ArrowGeometry(double yaw, double pitch, double morph) {
    static const double verts[5][3] = {{0.0, 0.0, 1.3}, {-0.62, 0.0, -0.7}, {0.62, 0.0, -0.7}, {0.0, 0.0, -0.3},
                                       {0.0, 0.28, -0.4}};
    static const int faces[6][3] = {{0, 1, 4}, {0, 4, 2}, {1, 3, 4}, {3, 2, 4}, {0, 3, 1}, {0, 2, 3}};
    const double kTilt = -0.9, kRing = 36.0, kRingIdle = kRing * 0.4, kSphere = 9.0;
    const double light[3] = {0.35, 0.75, 0.55};
    const double pi = 3.14159265358979323846;
    morph = std::max(0.0, std::min(1.0, morph));
    const double ry = yaw * pi / 180.0, rp = pitch * pi / 180.0;
    auto rot_y = [](const double* p, double a, double* o) {
        double c = std::cos(a), s = std::sin(a);
        o[0] = p[0] * c + p[2] * s;
        o[1] = p[1];
        o[2] = -p[0] * s + p[2] * c;
    };
    auto rot_x = [](const double* p, double a, double* o) {
        double c = std::cos(a), s = std::sin(a);
        o[0] = p[0];
        o[1] = p[1] * c - p[2] * s;
        o[2] = p[1] * s + p[2] * c;
    };
    double world[5][3], proj[5][2];
    for (int i = 0; i < 5; ++i) {
        double a[3], b[3];
        rot_y(verts[i], ry, a);
        rot_x(a, -rp, b);
        rot_x(b, kTilt, world[i]);
        double k = 26.0 * (1.0 - morph) / (1.0 + world[i][2] * 0.12);
        proj[i][0] = world[i][0] * k;
        proj[i][1] = -world[i][1] * k;
    }
    ArrowGeo g;
    if (morph < 0.98) {
        for (const auto& f : faces) {
            const double *a = world[f[0]], *b = world[f[1]], *c = world[f[2]];
            double u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, v[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
            double n[3] = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
            double m = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
            if (m == 0) m = 1.0;
            double lit = std::fabs((n[0] * light[0] + n[1] * light[1] + n[2] * light[2]) / m);
            ArrowFace af;
            af.z = (a[2] + b[2] + c[2]) / 3;
            for (int i = 0; i < 3; ++i) {
                af.pts[i * 2] = proj[f[i]][0];
                af.pts[i * 2 + 1] = proj[f[i]][1];
            }
            // Igual que Python: round() de Python redondea al par en .5 (std::nearbyint lo hace igual).
            af.r = static_cast<unsigned char>(std::nearbyint(170 + 80 * lit));
            af.g = static_cast<unsigned char>(std::nearbyint(105 + 80 * lit));
            af.b = static_cast<unsigned char>(std::nearbyint(15 + 50 * lit));
            g.faces.push_back(af);
        }
        std::stable_sort(g.faces.begin(), g.faces.end(), [](const ArrowFace& x, const ArrowFace& y) { return -x.z < -y.z; });
    }
    double r = kSphere * morph;
    if (r >= 0.3) {
        const double layers[4][2] = {{1.0, 0.0}, {0.72, 0.18}, {0.42, 0.32}, {0.16, 0.42}};
        const unsigned char cols[4][3] = {{0xB9, 0x7A, 0x10}, {0xE0, 0x9A, 0x22}, {0xF2, 0xB1, 0x34}, {0xFF, 0xE7, 0xA8}};
        for (int i = 0; i < 4; ++i)
            g.sphere.push_back(ArrowSphere{-r * layers[i][1], -r * layers[i][1], r * layers[i][0], cols[i][0], cols[i][1],
                                           cols[i][2]});
    }
    g.ring_r = kRing - (kRing - kRingIdle) * morph;
    return g;
}

}  // namespace olympian
