// Olympian Planet Coords (DLL) - recuadro dentro del juego (Vulkan), paso 2 de la v0.2
// Olympian Planet Coords (DLL) - in-game box (Vulkan), v0.2 step 2
//
// En cada vkQueuePresentKHR del juego, antes de mostrar la imagen:
//   1. arma el recuadro en la CPU (GDI: fondo, borde y texto) y lo deja en un bufer visible por la CPU;
//   2. graba un comando que copia ese bufer a la esquina superior izquierda de la imagen de la cadena
//      (PRESENT_SRC -> TRANSFER_DST -> copia -> PRESENT_SRC) y lo envia a la misma cola del juego,
//      esperando los semaforos del juego y senalando uno propio;
//   3. presenta esperando ese semaforo propio.
// No usa sombreadores ni pasadas de render: solo una copia, que la cadena del juego permite
// (uso 0x1e incluye TRANSFER_DST). Si algo no cuadra presenta sin tocar nada.
//
// Olympian Planet Coords — Copyright © 2026 MightyZeus · Licencia GPL-3.0

#include "vk_overlay.hpp"

#include "arrow.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <climits>
#include <cwctype>
#include <cwchar>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace olympian {
// Fuentes incluidas en el DLL (src/fonts.S).
extern "C" const unsigned char olympian_font_dseg7[], olympian_font_dseg7_end[];
extern "C" const unsigned char olympian_font_sharetech[], olympian_font_sharetech_end[];

namespace {

constexpr uint32_t kFrames = 4;                     // comandos en vuelo
constexpr VkDeviceSize kMaxBytes = 1280 * 1100 * 4;  // bufer por cuadro (el panel abierto en 4K cabe)
constexpr double kTextEveryMs = 50;  // al escribir en el panel, la letra aparece en <= 50 ms

OverlayLog g_log = nullptr;
OverlayTextFn g_text = nullptr;
PFN_vkGetPhysicalDeviceMemoryProperties g_mem_props = nullptr;
std::mutex g_mu;

struct Fns {
    PFN_vkGetDeviceQueue GetDeviceQueue = nullptr;
    PFN_vkCreateCommandPool CreateCommandPool = nullptr;
    PFN_vkAllocateCommandBuffers AllocateCommandBuffers = nullptr;
    PFN_vkBeginCommandBuffer BeginCommandBuffer = nullptr;
    PFN_vkEndCommandBuffer EndCommandBuffer = nullptr;
    PFN_vkResetCommandBuffer ResetCommandBuffer = nullptr;
    PFN_vkCmdPipelineBarrier CmdPipelineBarrier = nullptr;
    PFN_vkCmdCopyBufferToImage CmdCopyBufferToImage = nullptr;
    PFN_vkQueueSubmit QueueSubmit = nullptr;
    PFN_vkCreateFence CreateFence = nullptr;
    PFN_vkGetFenceStatus GetFenceStatus = nullptr;
    PFN_vkResetFences ResetFences = nullptr;
    PFN_vkCreateSemaphore CreateSemaphore = nullptr;
    PFN_vkCreateBuffer CreateBuffer = nullptr;
    PFN_vkGetBufferMemoryRequirements GetBufferMemoryRequirements = nullptr;
    PFN_vkAllocateMemory AllocateMemory = nullptr;
    PFN_vkBindBufferMemory BindBufferMemory = nullptr;
    PFN_vkMapMemory MapMemory = nullptr;
    PFN_vkGetSwapchainImagesKHR GetSwapchainImagesKHR = nullptr;
};

struct Frame {
    VkCommandBuffer cb = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkBuffer buf = VK_NULL_HANDLE;
    VkDeviceMemory mem = VK_NULL_HANDLE;
    uint8_t* map = nullptr;
    bool pending = false;
};

struct Pool {
    VkCommandPool pool = VK_NULL_HANDLE;
    std::vector<Frame> frames;
    uint32_t next = 0;
    bool ok = false;
};

struct Swap {
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent{0, 0};
    std::vector<VkImage> images;
    std::vector<VkSemaphore> sems;  // uno por imagen: se reutiliza cuando el juego vuelve a usar esa imagen
};

VkDevice g_dev = VK_NULL_HANDLE;
VkPhysicalDevice g_pd = VK_NULL_HANDLE;
Fns F;
uint32_t g_host_type = UINT32_MAX;
std::map<VkQueue, uint32_t> g_queue_family;
std::map<uint32_t, Pool> g_pools;
std::map<VkSwapchainKHR, Swap> g_swaps;
bool g_broken = false;

std::atomic<unsigned long long> g_drawn{0}, g_skipped_busy{0}, g_passthrough{0};
std::string g_last_error;

// Recuadro ya rasterizado (pixeles en el orden de la imagen de la cadena).
struct Raster {
    std::wstring text;
    uint32_t w = 0, h = 0;
    bool bgra = false;
    std::vector<uint32_t> px;
} g_raster;
ULONGLONG g_text_at = 0;
std::wstring g_text_now;

void Fail(const char* what, VkResult r) {
    g_broken = true;
    char buf[160];
    std::snprintf(buf, sizeof(buf), "%s -> %d", what, static_cast<int>(r));
    g_last_error = buf;
    if (g_log) g_log("[overlay] desactivado: %s", buf);
}

template <typename T>
void Load(PFN_vkGetDeviceProcAddr gdpa, VkDevice d, const char* name, T& out) {
    out = reinterpret_cast<T>(gdpa(d, name));
}

bool Supported(VkFormat f, bool* bgra) {
    switch (f) {
        case VK_FORMAT_R8G8B8A8_UNORM:
        case VK_FORMAT_R8G8B8A8_SRGB:
            *bgra = false;
            return true;
        case VK_FORMAT_B8G8R8A8_UNORM:
        case VK_FORMAT_B8G8R8A8_SRGB:
            *bgra = true;
            return true;
        default:
            return false;
    }
}

// Estilo de cada linea (codigos en vk_overlay.hpp). Tamanos en puntos, los mismos del recuadro .exe
// (FONT_SIZES de overlay/olympian_overlay.py): Normal = num/texto 8, pequena 6, casilla 8; Grande = 15,
// 10, 14. Como en el .exe, los puntos pasan a pixeles con el DPI de Windows (96 = 100 %), no con la
// resolucion. Sin codigo: linea 1 titulo, 2 grande, 3 pequena.
struct LineStyle {
    double pt_normal, pt_big;
    COLORREF color;
    bool bold;
    int fill;  // 0 nada, 1 fila elegida, 2 casilla de texto
};
struct Line {
    std::wstring text;
    LineStyle st;
    bool numeric = false;
    double pt = 8;      // puntos (segun Normal / Grande)
    double tag_pt = 6;  // etiqueta antes del tabulador (AHORA, DESTINO)
};
// Lo que se dibuja: lineas, si es el panel, tamano Grande y la flecha.
struct Layout {
    std::vector<Line> lines;
    bool panel = false;
    bool big = false;
    bool arrow = false;
    double yaw = 0, pitch = 0, morph = 1;
};

// Colores del recuadro .exe
constexpr COLORREF kFg = RGB(0xE8, 0xF1, 0xF2), kMuted = RGB(0x88, 0x99, 0xA6), kAccent = RGB(0xF2, 0xB1, 0x34),
                   kDestColor = RGB(0x7F, 0xD4, 0xFF);

LineStyle StyleFor(wchar_t code, size_t index, bool* coded) {
    *coded = true;
    switch (code) {
        case kStyleTitle: return {6, 10, kAccent, false, 0};
        case kStyleBig: return {8, 15, kFg, false, 0};
        case kStyleSmall: return {6, 10, kMuted, false, 0};
        case kStyleItem: return {8, 14, kFg, false, 0};
        case kStyleSelected: return {8, 14, RGB(255, 255, 255), false, 1};
        case kStyleError: return {6, 10, RGB(240, 100, 100), false, 0};
        case kStyleLabel: return {6, 10, kAccent, false, 0};
        case kStyleInput: return {8, 14, kFg, false, 2};
        case kStyleOk: return {6, 10, RGB(120, 210, 140), false, 0};
        case kStyleDest: return {8, 15, kDestColor, false, 0};
        case kStyleDestSmall: return {6, 10, kDestColor, false, 0};
        default: break;
    }
    *coded = false;
    if (index == 0) return {6, 10, kAccent, false, 0};
    if (index == 1) return {8, 15, kFg, false, 0};
    return {6, 10, kMuted, false, 0};
}

// Lineas del texto. Sin codigos de estilo: maximo 3 (texto simple). Con codigos: hasta 24. Las lineas
// de control (kBoxBig, kBoxArrow) no se dibujan como texto.
Layout ParseLayout(const std::wstring& text) {
    Layout L;
    size_t a = 0;
    bool styled = false;
    for (;;) {
        size_t b = text.find(L'\n', a);
        std::wstring raw = text.substr(a, b == std::wstring::npos ? std::wstring::npos : b - a);
        if (!raw.empty() && raw[0] == kBoxBig) {
            L.big = raw.size() > 1 && raw[1] == L'G';
        } else if (!raw.empty() && raw[0] == kBoxArrow) {
            double y = 0, p = 0, m = 1;
            if (swscanf(raw.c_str() + 1, L"%lf;%lf;%lf", &y, &p, &m) == 3) {
                L.arrow = true;
                L.yaw = y;
                L.pitch = p;
                L.morph = m;
            }
        } else {
            Line l;
            l.text = raw;
            bool coded = false;
            l.st = StyleFor(l.text.empty() ? 0 : l.text[0], L.lines.size(), &coded);
            if (coded) {
                l.text.erase(0, 1);
                styled = true;
                if (l.st.fill || raw[0] == kStyleItem) L.panel = true;  // filas o casilla: es el panel
            }
            if (!l.text.empty() && l.text[0] == kNumericLine) {
                l.numeric = true;
                l.text.erase(0, 1);
            }
            L.lines.push_back(std::move(l));
        }
        if (b == std::wstring::npos || L.lines.size() == 24) break;
        a = b + 1;
    }
    if (!styled && L.lines.size() > 3) L.lines.resize(3);
    for (auto& l : L.lines) {
        l.pt = L.big ? l.st.pt_big : l.st.pt_normal;
        l.tag_pt = L.big ? 10 : 6;
    }
    return L;
}

// Puntos -> pixeles a la escala s (s = DPI / 96).
int Px(double pt, double s) { return std::max(6, static_cast<int>(std::lround(pt * s * 96.0 / 72.0))); }

// Alto de una linea (como el de una etiqueta de tk: el alto de la letra), mas margen si tiene fondo.
int LineStep(const Line& l, double s) {
    return static_cast<int>(std::lround(Px(l.pt, s) * 1.32)) + (l.st.fill ? static_cast<int>(std::lround(6 * s)) : 0);
}

// DPI de Windows (el mismo que usaba el recuadro .exe para pasar puntos a pixeles). El juego puede
// no ser "consciente del DPI" (Windows le diria 96), asi que primero se lee el valor aplicado.
double SystemDpi() {
    static double dpi = 0;
    if (dpi > 0) return dpi;
    DWORD v = 0, sz = sizeof(v);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Control Panel\\Desktop\\WindowMetrics", L"AppliedDPI", RRF_RT_REG_DWORD,
                     nullptr, &v, &sz) == ERROR_SUCCESS &&
        v >= 72 && v <= 480) {
        dpi = v;
    } else if (HMODULE u = GetModuleHandleW(L"user32")) {
        using Fn = UINT(WINAPI*)();
        auto f = reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(u, "GetDpiForSystem")));
        dpi = f ? f() : 96;
    }
    if (dpi < 72 || dpi > 480) dpi = 96;
    if (g_log) g_log("[overlay] DPI de Windows: %.0f (escala %.0f %%)", dpi, dpi / 96.0 * 100);
    return dpi;
}

// Fuentes privadas del proceso (no se instalan en Windows). Se cargan una vez.
bool g_fonts_tried = false, g_font_num = false, g_font_text = false;
void EnsureFonts() {
    if (g_fonts_tried) return;
    g_fonts_tried = true;
    DWORD n = 0;
    g_font_num = AddFontMemResourceEx(const_cast<unsigned char*>(olympian_font_dseg7),
                                      static_cast<DWORD>(olympian_font_dseg7_end - olympian_font_dseg7), nullptr, &n) &&
                 n > 0;
    n = 0;
    g_font_text = AddFontMemResourceEx(const_cast<unsigned char*>(olympian_font_sharetech),
                                       static_cast<DWORD>(olympian_font_sharetech_end - olympian_font_sharetech),
                                       nullptr, &n) &&
                  n > 0;
    if (g_log) g_log("[overlay] fuentes: DSEG7 Classic %s, Share Tech Mono %s", g_font_num ? "OK" : "NO",
                     g_font_text ? "OK" : "NO");
}

const wchar_t* NumFamily() { return g_font_num ? L"DSEG7 Classic" : L"Consolas"; }

// Texto con cirilico: Consolas (Share Tech Mono no lo tiene). Si no, Share Tech Mono.
const wchar_t* TextFamily(const std::wstring& t) {
    for (wchar_t c : t)
        if (c >= 0x0400 && c <= 0x04FF) return L"Consolas";
    return g_font_text ? L"Share Tech Mono" : L"Consolas";
}

HFONT MakeFont(int px, bool bold, const wchar_t* family) {
    return CreateFontW(-px, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, family);
}

// Dibuja una linea (o solo la mide si draw es false). Devuelve el ancho. Como en el .exe: digitos y
// punto en DSEG7 (negrita), lo demas en Share Tech Mono; las letras que esa fuente no tiene (flechas,
// otros alfabetos) van en Segoe UI, que Windows completa con sus fuentes de reserva. Lo que va antes de
// un tabulador es la etiqueta (AHORA, DESTINO): pequena, gris y de 8 letras de ancho, como en el .exe.
int DrawLine(HDC dc, const Line& l, double s, int x0, int y, int right, int clip_bottom, bool draw) {
    std::wstring line = l.text, tag;
    size_t tab = line.find(L'\t');
    if (tab != std::wstring::npos) {
        tag = line.substr(0, tab);
        line.erase(0, tab + 1);
    }
    const LineStyle& st = l.st;
    const int px = Px(l.pt, s);
    HFONT ft = MakeFont(px, st.bold, TextFamily(line));
    HFONT fn = l.numeric ? MakeFont(px, true, NumFamily()) : nullptr;
    HGDIOBJ of = SelectObject(dc, ft);
    TEXTMETRICW tm{};
    GetTextMetricsW(dc, &tm);
    int ascent = tm.tmAscent;
    if (fn) {  // DSEG7 sube mas que Share Tech Mono (ascenso 1.0 contra 0.885): la linea base baja para que
               // no se corte la parte de arriba de los digitos (el 0 se veia como una U)
        SelectObject(dc, fn);
        TEXTMETRICW tn{};
        GetTextMetricsW(dc, &tn);
        ascent = std::max<int>(ascent, tn.tmAscent);
        SelectObject(dc, ft);
    }
    const int baseline = y + ascent;
    SetTextAlign(dc, TA_LEFT | TA_BASELINE | TA_NOUPDATECP);
    int x = x0;
    if (tab != std::wstring::npos) {
        HFONT fa = MakeFont(Px(l.tag_pt, s), false, TextFamily(tag));
        SelectObject(dc, fa);
        SIZE cell{};
        GetTextExtentPoint32W(dc, L"00000000", 8, &cell);
        if (draw) {
            SetTextColor(dc, kMuted);
            RECT clip{x, y, right, clip_bottom};
            ExtTextOutW(dc, x, baseline, ETO_CLIPPED, &clip, tag.c_str(), static_cast<UINT>(tag.size()), nullptr);
        }
        SIZE tsz{};
        GetTextExtentPoint32W(dc, tag.c_str(), static_cast<int>(tag.size()), &tsz);
        x += std::max(cell.cx, tsz.cx + static_cast<int>(std::lround(4 * s)));
        SelectObject(dc, ft);
        DeleteObject(fa);
    }
    SetTextColor(dc, st.color);
    std::vector<WORD> glyphs(line.size() + 1, 0);
    if (!line.empty())
        GetGlyphIndicesW(dc, line.c_str(), static_cast<int>(line.size()), glyphs.data(), GGI_MARK_NONEXISTING_GLYPHS);
    HFONT fb = nullptr;
    // Las coordenadas terminan en el primer doble espacio: lo que sigue (el nombre) va en letra normal.
    const size_t num_end = line.find(L"  ");
    auto cls = [&](size_t k) {  // 0 = digito DSEG7, 1 = fuente de texto, 2 = reserva
        if (l.numeric && fn && k < num_end && (iswdigit(line[k]) || line[k] == L'.')) return 0;
        return glyphs[k] == 0xFFFF ? 2 : 1;
    };
    size_t k = 0;
    while (k < line.size() && x < right) {
        int c = cls(k);
        size_t e = k + 1;
        while (e < line.size() && cls(e) == c) ++e;
        if (c == 2 && !fb) fb = MakeFont(px, st.bold, L"Segoe UI");
        SelectObject(dc, c == 0 ? fn : c == 2 && fb ? fb : ft);
        SIZE sz{};
        GetTextExtentPoint32W(dc, line.c_str() + k, static_cast<int>(e - k), &sz);
        if (draw) {
            RECT clip{x, 0, right, clip_bottom};  // solo se corta a la derecha y abajo
            ExtTextOutW(dc, x, baseline, ETO_CLIPPED, &clip, line.c_str() + k, static_cast<UINT>(e - k), nullptr);
        }
        x += sz.cx;
        k = e;
    }
    if (fb) DeleteObject(fb);
    SelectObject(dc, of);
    DeleteObject(ft);
    if (fn) DeleteObject(fn);
    return x - x0;
}

// Margenes (como el marco del .exe: 10 px a los lados, 6 arriba y abajo), mas la barra de color.
int PadX(double s) { return static_cast<int>(std::lround(14 * s)); }  // barra 4 + 10
int PadY(double s) { return static_cast<int>(std::lround(6 * s)); }
int ContentHeight(const Layout& L, double s) {
    int hh = 0;
    for (const auto& l : L.lines) hh += LineStep(l, s);
    return hh;
}
// La flecha: un cuadrado del alto del texto (en el .exe, del alto del recuadro), minimo 24 px.
int ArrowSize(const Layout& L, double s) {
    return L.arrow ? std::max(static_cast<int>(std::lround(24 * s)), ContentHeight(L, s)) : 0;
}

// Medidas del recuadro a escala s: ancho justo para el texto (y la flecha), alto segun las lineas.
void Measure(const Layout& L, double s, int* w, int* h) {
    EnsureFonts();
    static HDC dc = CreateCompatibleDC(nullptr);
    int maxw = 0;
    for (const auto& l : L.lines) maxw = std::max(maxw, DrawLine(dc, l, s, 0, 0, INT_MAX / 2, 0, false));
    const int a = ArrowSize(L, s);
    *w = PadX(s) + maxw + (a ? static_cast<int>(std::lround(8 * s)) + a : 0) + static_cast<int>(std::lround(10 * s));
    *h = 2 * PadY(s) + std::max(ContentHeight(L, s), a);
}

// Tamano del recuadro: DPI de Windows (como el .exe), tamano elegido y texto. Si no cabe en la imagen o
// en el bufer, se achica todo junto.
void BoxSize(VkExtent2D e, const std::wstring& text, uint32_t* w, uint32_t* h, double* scale) {
    static std::wstring last_text;
    static VkExtent2D last_e{};
    static uint32_t lw = 0, lh = 0;
    static double ls = 1;
    if (text == last_text && e.width == last_e.width && e.height == last_e.height && lw) {
        *w = lw;
        *h = lh;
        *scale = ls;
        return;
    }
    Layout L = ParseLayout(text);
    double s = SystemDpi() / 96.0;
    int iw = 0, ih = 0;
    for (int tries = 0; tries < 30; ++tries) {
        Measure(L, s, &iw, &ih);
        bool fits = iw <= static_cast<int>(e.width) * 0.9 && ih <= static_cast<int>(e.height) * 0.9 &&
                    static_cast<VkDeviceSize>(iw) * ih * 4 <= kMaxBytes;
        if (fits || s <= 0.5) break;
        s *= 0.9;
    }
    *w = std::min<uint32_t>(static_cast<uint32_t>(iw), e.width);
    *h = std::min<uint32_t>(static_cast<uint32_t>(ih), e.height);
    *scale = s;
    last_text = text;
    last_e = e;
    lw = *w;
    lh = *h;
    ls = s;
}

// Flecha 3D (igual que la del recuadro .exe): se dibuja a 4x con GDI y se reduce (bordes suaves).
void DrawArrow(uint32_t* p, uint32_t w, uint32_t h, int ox, int oy, int size, const Layout& L) {
    if (size < 8) return;
    const int ss = 4, big = size * ss;
    HDC dc = CreateCompatibleDC(nullptr);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = big;
    bi.bmiHeader.biHeight = -big;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = dc ? CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0) : nullptr;
    if (!bmp || !bits) {
        if (bmp) DeleteObject(bmp);
        if (dc) DeleteDC(dc);
        return;
    }
    HGDIOBJ old = SelectObject(dc, bmp);
    auto* q = static_cast<uint32_t*>(bits);
    for (int i = 0; i < big * big; ++i) q[i] = 0x14181E;  // fondo del recuadro
    olympian::ArrowGeo g = olympian::ArrowGeometry(L.yaw, L.pitch, L.morph);
    const double k = big / 80.0, c = big / 2.0, px = size / 80.0;
    int ring_w = std::max(ss, static_cast<int>(std::lround(1.8 * px * ss)));
    HPEN ring = CreatePen(PS_SOLID, ring_w, RGB(0xA8, 0x74, 0x0F));
    HGDIOBJ op = SelectObject(dc, ring);
    HGDIOBJ ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
    int rr = static_cast<int>(std::lround(g.ring_r * k));
    Ellipse(dc, static_cast<int>(c) - rr, static_cast<int>(c) - rr, static_cast<int>(c) + rr, static_cast<int>(c) + rr);
    HPEN edge = CreatePen(PS_SOLID, std::max(1, static_cast<int>(std::lround(0.8 * px * ss))), RGB(0xFF, 0xE7, 0xA8));
    SelectObject(dc, edge);
    for (const auto& f : g.faces) {
        POINT pts[3];
        for (int i = 0; i < 3; ++i)
            pts[i] = POINT{static_cast<LONG>(std::lround(c + f.pts[i * 2] * k)),
                           static_cast<LONG>(std::lround(c + f.pts[i * 2 + 1] * k))};
        HBRUSH br = CreateSolidBrush(RGB(f.r, f.g, f.b));
        HGDIOBJ obr = SelectObject(dc, br);
        Polygon(dc, pts, 3);
        SelectObject(dc, obr);
        DeleteObject(br);
    }
    SelectObject(dc, GetStockObject(NULL_PEN));
    for (const auto& sph : g.sphere) {
        HBRUSH br = CreateSolidBrush(RGB(sph.red, sph.green, sph.blue));
        HGDIOBJ obr = SelectObject(dc, br);
        double x = c + sph.cx * k, y = c + sph.cy * k, r = sph.r * k;
        Ellipse(dc, static_cast<int>(std::lround(x - r)), static_cast<int>(std::lround(y - r)),
                static_cast<int>(std::lround(x + r)), static_cast<int>(std::lround(y + r)));
        SelectObject(dc, obr);
        DeleteObject(br);
    }
    SelectObject(dc, op);
    SelectObject(dc, ob);
    DeleteObject(ring);
    DeleteObject(edge);
    GdiFlush();
    for (int y = 0; y < size; ++y) {  // reducir 4x4 -> 1 (promedio)
        int py = oy + y;
        if (py < 1 || py + 1 >= static_cast<int>(h)) continue;
        for (int x = 0; x < size; ++x) {
            int pxx = ox + x;
            if (pxx < 1 || pxx + 1 >= static_cast<int>(w)) continue;
            unsigned r = 0, gg = 0, b = 0;
            for (int j = 0; j < ss; ++j)
                for (int i = 0; i < ss; ++i) {
                    uint32_t v = q[(y * ss + j) * big + x * ss + i];
                    r += (v >> 16) & 0xFF;
                    gg += (v >> 8) & 0xFF;
                    b += v & 0xFF;
                }
            const unsigned n = ss * ss;
            p[py * w + pxx] = ((r / n) << 16) | ((gg / n) << 8) | (b / n);
        }
    }
    SelectObject(dc, old);
    DeleteObject(bmp);
    DeleteDC(dc);
}

void Rasterize(const std::wstring& text, uint32_t w, uint32_t h, double s, bool bgra) {
    g_raster.text = text;
    g_raster.w = w;
    g_raster.h = h;
    g_raster.bgra = bgra;
    g_raster.px.assign(static_cast<size_t>(w) * h, 0);
    HDC dc = CreateCompatibleDC(nullptr);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = static_cast<LONG>(w);
    bi.bmiHeader.biHeight = -static_cast<LONG>(h);  // de arriba hacia abajo
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = dc ? CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0) : nullptr;
    if (!bmp || !bits) {
        if (bmp) DeleteObject(bmp);
        if (dc) DeleteDC(dc);
        return;
    }
    HGDIOBJ old_bmp = SelectObject(dc, bmp);
    auto* p = static_cast<uint32_t*>(bits);
    const uint32_t bg = 0x14181E, edge = 0x3A4048, accent = 0xE8A33D;  // 0x00RRGGBB en el DIB
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x) {
            bool border = x == 0 || y == 0 || x + 1 == w || y + 1 == h;
            bool bar = x < static_cast<uint32_t>(std::max(3.0, 4 * s));
            p[y * w + x] = bar ? accent : border ? edge : bg;
        }
    SetBkMode(dc, TRANSPARENT);
    EnsureFonts();
    Layout L = ParseLayout(text);
    const int arrow_px = ArrowSize(L, s);
    const int x0 = PadX(s);
    const int text_right =
        static_cast<int>(w) - static_cast<int>(std::lround(6 * s)) - (arrow_px ? arrow_px + static_cast<int>(std::lround(8 * s)) : 0);
    int y = PadY(s);
    for (size_t i = 0; i < L.lines.size(); ++i) {
        const Line& line = L.lines[i];
        const LineStyle& st = line.st;
        const int step = LineStep(line, s);
        int ty = y;
        if (st.fill) {  // fila elegida o casilla de texto: fondo propio
            const uint32_t fc = st.fill == 1 ? 0x3A4660 : 0x07090D;
            const uint32_t fe = st.fill == 1 ? 0xE8A33D : 0x5A6270;
            int y0 = y + static_cast<int>(std::lround(1 * s)), y1 = std::min<int>(y + step - static_cast<int>(std::lround(1 * s)), static_cast<int>(h) - 2);
            int xa = x0 - static_cast<int>(std::lround(5 * s)), xb = static_cast<int>(w) - static_cast<int>(std::lround(6 * s));
            for (int yy = std::max(1, y0); yy < y1; ++yy)
                for (int xx = std::max(1, xa); xx < xb; ++xx) {
                    bool ed = st.fill == 1 ? xx < xa + static_cast<int>(std::lround(3 * s))
                                           : (yy == std::max(1, y0) || yy + 1 == y1 || xx == std::max(1, xa) || xx + 1 == xb);
                    p[yy * w + xx] = ed ? fe : fc;
                }
            ty = y + static_cast<int>(std::lround(3 * s));
        }
        DrawLine(dc, line, s, x0, ty, i == 0 ? static_cast<int>(w) - x0 / 2 : text_right, static_cast<int>(h), true);
        y += step;
    }
    GdiFlush();
    if (arrow_px)  // a la derecha, centrada en el alto del recuadro
        DrawArrow(p, w, h, static_cast<int>(w) - static_cast<int>(std::lround(8 * s)) - arrow_px,
                  (static_cast<int>(h) - arrow_px) / 2, arrow_px, L);
    for (size_t i = 0; i < g_raster.px.size(); ++i) {
        uint32_t c = p[i] & 0x00FFFFFF;  // GDI deja el alfa en 0: el recuadro es opaco
        if (!bgra) c = (c & 0x0000FF00) | ((c & 0xFF) << 16) | ((c >> 16) & 0xFF);
        g_raster.px[i] = c | 0xFF000000;
    }
    SelectObject(dc, old_bmp);
    DeleteObject(bmp);
    DeleteDC(dc);
}

bool MakePool(uint32_t family, Pool& p) {
    VkCommandPoolCreateInfo pci{};
    pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pci.queueFamilyIndex = family;
    VkResult r = F.CreateCommandPool(g_dev, &pci, nullptr, &p.pool);
    if (r != VK_SUCCESS) return Fail("vkCreateCommandPool", r), false;
    p.frames.resize(kFrames);
    for (auto& fr : p.frames) {
        VkCommandBufferAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool = p.pool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;
        if ((r = F.AllocateCommandBuffers(g_dev, &ai, &fr.cb)) != VK_SUCCESS) return Fail("vkAllocateCommandBuffers", r), false;
        VkFenceCreateInfo fci{};
        fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        if ((r = F.CreateFence(g_dev, &fci, nullptr, &fr.fence)) != VK_SUCCESS) return Fail("vkCreateFence", r), false;
        VkBufferCreateInfo bci{};
        bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bci.size = kMaxBytes;
        bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if ((r = F.CreateBuffer(g_dev, &bci, nullptr, &fr.buf)) != VK_SUCCESS) return Fail("vkCreateBuffer", r), false;
        VkMemoryRequirements mr{};
        F.GetBufferMemoryRequirements(g_dev, fr.buf, &mr);
        uint32_t type = UINT32_MAX;
        if (g_mem_props) {
            VkPhysicalDeviceMemoryProperties mp{};
            g_mem_props(g_pd, &mp);
            const VkMemoryPropertyFlags want =
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
                if ((mr.memoryTypeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want) {
                    type = i;
                    break;
                }
        }
        if (type == UINT32_MAX) return Fail("memoria visible por la CPU no encontrada", VK_ERROR_FEATURE_NOT_PRESENT), false;
        g_host_type = type;
        VkMemoryAllocateInfo mai{};
        mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize = mr.size;
        mai.memoryTypeIndex = type;
        if ((r = F.AllocateMemory(g_dev, &mai, nullptr, &fr.mem)) != VK_SUCCESS) return Fail("vkAllocateMemory", r), false;
        if ((r = F.BindBufferMemory(g_dev, fr.buf, fr.mem, 0)) != VK_SUCCESS) return Fail("vkBindBufferMemory", r), false;
        void* m = nullptr;
        if ((r = F.MapMemory(g_dev, fr.mem, 0, VK_WHOLE_SIZE, 0, &m)) != VK_SUCCESS) return Fail("vkMapMemory", r), false;
        fr.map = static_cast<uint8_t*>(m);
    }
    p.ok = true;
    if (g_log) g_log("[overlay] recursos listos: familia de cola %u, %u cuadros, memoria tipo %u", family, kFrames, g_host_type);
    return true;
}

}  // namespace

void OverlaySetup(OverlayLog log, OverlayTextFn text, PFN_vkGetPhysicalDeviceMemoryProperties mem_props) {
    std::lock_guard<std::mutex> g(g_mu);
    g_log = log;
    g_text = text;
    g_mem_props = mem_props;
}

void OverlayOnDevice(VkPhysicalDevice pd, VkDevice dev, const VkDeviceCreateInfo* ci, PFN_vkGetDeviceProcAddr gdpa) {
    if (!dev || !gdpa || !ci) return;
    std::lock_guard<std::mutex> g(g_mu);
    if (g_dev && g_dev != dev) {
        if (g_log) g_log("[overlay] segundo dispositivo de Vulkan: se ignora (se dibuja solo en el primero)");
        return;
    }
    g_dev = dev;
    g_pd = pd;
    Load(gdpa, dev, "vkGetDeviceQueue", F.GetDeviceQueue);
    Load(gdpa, dev, "vkCreateCommandPool", F.CreateCommandPool);
    Load(gdpa, dev, "vkAllocateCommandBuffers", F.AllocateCommandBuffers);
    Load(gdpa, dev, "vkBeginCommandBuffer", F.BeginCommandBuffer);
    Load(gdpa, dev, "vkEndCommandBuffer", F.EndCommandBuffer);
    Load(gdpa, dev, "vkResetCommandBuffer", F.ResetCommandBuffer);
    Load(gdpa, dev, "vkCmdPipelineBarrier", F.CmdPipelineBarrier);
    Load(gdpa, dev, "vkCmdCopyBufferToImage", F.CmdCopyBufferToImage);
    Load(gdpa, dev, "vkQueueSubmit", F.QueueSubmit);
    Load(gdpa, dev, "vkCreateFence", F.CreateFence);
    Load(gdpa, dev, "vkGetFenceStatus", F.GetFenceStatus);
    Load(gdpa, dev, "vkResetFences", F.ResetFences);
    Load(gdpa, dev, "vkCreateSemaphore", F.CreateSemaphore);
    Load(gdpa, dev, "vkCreateBuffer", F.CreateBuffer);
    Load(gdpa, dev, "vkGetBufferMemoryRequirements", F.GetBufferMemoryRequirements);
    Load(gdpa, dev, "vkAllocateMemory", F.AllocateMemory);
    Load(gdpa, dev, "vkBindBufferMemory", F.BindBufferMemory);
    Load(gdpa, dev, "vkMapMemory", F.MapMemory);
    Load(gdpa, dev, "vkGetSwapchainImagesKHR", F.GetSwapchainImagesKHR);
    void* const* all = reinterpret_cast<void* const*>(&F);
    for (size_t i = 0; i < sizeof(F) / sizeof(void*); ++i)
        if (!all[i]) return Fail("falta una funcion de Vulkan", VK_ERROR_INITIALIZATION_FAILED);
    // Colas que creo el juego, para saber a que familia pertenece la que presenta.
    for (uint32_t i = 0; i < ci->queueCreateInfoCount; ++i) {
        const VkDeviceQueueCreateInfo& q = ci->pQueueCreateInfos[i];
        if (q.flags) continue;  // colas protegidas: vkGetDeviceQueue no sirve para ellas
        for (uint32_t k = 0; k < q.queueCount; ++k) {
            VkQueue h = VK_NULL_HANDLE;
            F.GetDeviceQueue(dev, q.queueFamilyIndex, k, &h);
            if (h) g_queue_family[h] = q.queueFamilyIndex;
        }
    }
    if (g_log) g_log("[overlay] dispositivo listo: %zu colas conocidas", g_queue_family.size());
}

void OverlayOnSwapchain(VkDevice dev, const VkSwapchainCreateInfoKHR* ci, VkSwapchainKHR sc) {
    if (!ci || !sc) return;
    std::lock_guard<std::mutex> g(g_mu);
    if (dev != g_dev || g_broken) return;
    Swap s;
    s.format = ci->imageFormat;
    s.extent = ci->imageExtent;
    bool bgra = false;
    if (!Supported(s.format, &bgra) || !(ci->imageUsage & VK_IMAGE_USAGE_TRANSFER_DST_BIT)) {
        if (g_log) g_log("[overlay] cadena %ux%u formato=%d uso=0x%x: no se puede copiar ahi, no se dibuja",
                         s.extent.width, s.extent.height, static_cast<int>(s.format), ci->imageUsage);
        g_swaps[sc] = Swap{};  // marcada como no apta
        return;
    }
    uint32_t n = 0;
    VkResult r = F.GetSwapchainImagesKHR(dev, sc, &n, nullptr);
    if (r == VK_SUCCESS && n) {
        s.images.resize(n);
        r = F.GetSwapchainImagesKHR(dev, sc, &n, s.images.data());
    }
    if (r != VK_SUCCESS && r != VK_INCOMPLETE) return Fail("vkGetSwapchainImagesKHR", r);
    s.images.resize(n);
    s.sems.assign(n, VK_NULL_HANDLE);
    for (auto& sem : s.sems) {
        VkSemaphoreCreateInfo sci{};
        sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        if ((r = F.CreateSemaphore(dev, &sci, nullptr, &sem)) != VK_SUCCESS) return Fail("vkCreateSemaphore", r);
    }
    if (g_log) g_log("[overlay] cadena %ux%u formato=%d con %u imagenes: se dibuja", s.extent.width, s.extent.height,
                     static_cast<int>(s.format), n);
    g_swaps[sc] = std::move(s);
}

void OverlayOnDestroySwapchain(VkSwapchainKHR sc) {
    std::lock_guard<std::mutex> g(g_mu);
    g_swaps.erase(sc);  // sus semaforos quedan sin uso (pocos, se recrean pocas veces)
}

VkResult OverlayPresent(VkQueue q, const VkPresentInfoKHR* pi, PFN_vkQueuePresentKHR real) {
    std::unique_lock<std::mutex> g(g_mu);
    auto pass = [&]() {
        ++g_passthrough;
        g.unlock();
        return real(q, pi);
    };
    if (g_broken || !g_dev || !pi || pi->swapchainCount != 1 || !pi->pImageIndices) return pass();
    auto si = g_swaps.find(pi->pSwapchains[0]);
    if (si == g_swaps.end() || si->second.images.empty()) return pass();
    Swap& sw = si->second;
    uint32_t idx = pi->pImageIndices[0];
    if (idx >= sw.images.size()) return pass();
    auto qf = g_queue_family.find(q);
    if (qf == g_queue_family.end()) return pass();

    // Texto (cada 100 ms) y recuadro rasterizado.
    ULONGLONG now = GetTickCount64();
    if (g_text && (now - g_text_at >= kTextEveryMs || g_text_at == 0)) {
        g_text_now = g_text();
        g_text_at = now;
    }
    if (g_text_now.empty()) return pass();
    uint32_t w, h;
    double s;
    BoxSize(sw.extent, g_text_now, &w, &h, &s);
    bool bgra = false;
    Supported(sw.format, &bgra);
    if (g_raster.text != g_text_now || g_raster.w != w || g_raster.h != h || g_raster.bgra != bgra || g_raster.px.empty())
        Rasterize(g_text_now, w, h, s, bgra);
    if (g_raster.px.empty()) return pass();

    Pool& pool = g_pools[qf->second];
    if (!pool.ok && !MakePool(qf->second, pool)) return pass();
    Frame& fr = pool.frames[pool.next];
    if (fr.pending) {
        if (F.GetFenceStatus(g_dev, fr.fence) != VK_SUCCESS) {  // la GPU todavia no termino: no esperar
            ++g_skipped_busy;
            return pass();
        }
        F.ResetFences(g_dev, 1, &fr.fence);
        fr.pending = false;
    }
    std::memcpy(fr.map, g_raster.px.data(), g_raster.px.size() * 4);

    const int32_t margin = static_cast<int32_t>(std::lround(24 * s));
    VkResult r = F.ResetCommandBuffer(fr.cb, 0);
    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (r == VK_SUCCESS) r = F.BeginCommandBuffer(fr.cb, &bi);
    if (r != VK_SUCCESS) {
        Fail("vkBeginCommandBuffer", r);
        return pass();
    }
    VkImageMemoryBarrier b{};
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    b.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = sw.images[idx];
    b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    F.CmdPipelineBarrier(fr.cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &b);
    VkBufferImageCopy c{};
    c.bufferOffset = 0;
    c.bufferRowLength = w;
    c.bufferImageHeight = h;
    c.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    c.imageOffset = {std::min<int32_t>(margin, static_cast<int32_t>(sw.extent.width - w)),
                     std::min<int32_t>(margin, static_cast<int32_t>(sw.extent.height - h)), 0};
    c.imageExtent = {w, h, 1};
    F.CmdCopyBufferToImage(fr.cb, fr.buf, sw.images[idx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &c);
    b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    b.dstAccessMask = 0;
    b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    b.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    F.CmdPipelineBarrier(fr.cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &b);
    if ((r = F.EndCommandBuffer(fr.cb)) != VK_SUCCESS) {
        Fail("vkEndCommandBuffer", r);
        return pass();
    }
    std::vector<VkPipelineStageFlags> stages(pi->waitSemaphoreCount, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    VkSemaphore mine = sw.sems[idx];
    VkSubmitInfo sub{};
    sub.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    sub.waitSemaphoreCount = pi->waitSemaphoreCount;
    sub.pWaitSemaphores = pi->pWaitSemaphores;
    sub.pWaitDstStageMask = stages.empty() ? nullptr : stages.data();
    sub.commandBufferCount = 1;
    sub.pCommandBuffers = &fr.cb;
    sub.signalSemaphoreCount = 1;
    sub.pSignalSemaphores = &mine;
    if ((r = F.QueueSubmit(q, 1, &sub, fr.fence)) != VK_SUCCESS) {
        Fail("vkQueueSubmit", r);  // nada se ejecuto: los semaforos del juego siguen pendientes
        return pass();
    }
    fr.pending = true;
    pool.next = (pool.next + 1) % kFrames;
    ++g_drawn;
    VkPresentInfoKHR p2 = *pi;
    p2.waitSemaphoreCount = 1;
    p2.pWaitSemaphores = &mine;
    g.unlock();
    return real(q, &p2);
}

std::string OverlayJson() {
    std::lock_guard<std::mutex> g(g_mu);
    char buf[512];
    std::snprintf(buf, sizeof(buf),
                  "{\"device\":%s,\"swapchains\":%zu,\"queues\":%zu,\"drawn\":%llu,\"skipped_busy\":%llu,"
                  "\"passthrough\":%llu,\"box\":[%u,%u],\"broken\":%s,\"error\":\"%s\"}",
                  g_dev ? "true" : "false", g_swaps.size(), g_queue_family.size(), g_drawn.load(),
                  g_skipped_busy.load(), g_passthrough.load(), g_raster.w, g_raster.h, g_broken ? "true" : "false",
                  g_last_error.c_str());
    return buf;
}

}  // namespace olympian
