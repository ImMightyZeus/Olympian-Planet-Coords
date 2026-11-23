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

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace olympian {
namespace {

constexpr uint32_t kFrames = 4;                     // comandos en vuelo
constexpr VkDeviceSize kMaxBytes = 1024 * 320 * 4;  // bufer por cuadro (cubre 4K)
constexpr double kTextEveryMs = 100;

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

// Tamano del recuadro segun la altura de la imagen (1080 = escala 1).
void BoxSize(VkExtent2D e, uint32_t* w, uint32_t* h, double* scale) {
    double s = std::clamp(e.height / 1080.0, 0.75, 2.5);
    *scale = s;
    *w = std::min<uint32_t>(static_cast<uint32_t>(std::lround(400 * s)), e.width);
    *h = std::min<uint32_t>(static_cast<uint32_t>(std::lround(96 * s)), e.height);
    while (static_cast<VkDeviceSize>(*w) * *h * 4 > kMaxBytes) {
        *w = *w * 9 / 10;
        *h = *h * 9 / 10;
    }
}

// Dibuja el recuadro con GDI en un DIB de 32 bits y lo convierte al orden de la cadena.
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
    std::vector<std::wstring> lines;
    size_t a = 0;
    for (;;) {
        size_t b = text.find(L'\n', a);
        lines.push_back(text.substr(a, b == std::wstring::npos ? std::wstring::npos : b - a));
        if (b == std::wstring::npos || lines.size() == 3) break;
        a = b + 1;
    }
    const int sizes[3] = {14, 24, 17};
    const COLORREF colors[3] = {RGB(232, 163, 61), RGB(240, 240, 240), RGB(176, 176, 176)};
    const int weights[3] = {FW_BOLD, FW_SEMIBOLD, FW_NORMAL};
    int y = static_cast<int>(std::lround(8 * s));
    const int x = static_cast<int>(std::lround(14 * s));
    for (size_t i = 0; i < lines.size(); ++i) {
        int px = static_cast<int>(std::lround(sizes[i] * s));
        HFONT f = CreateFontW(-px, 0, 0, 0, weights[i], 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        HGDIOBJ of = SelectObject(dc, f);
        SetTextColor(dc, colors[i]);
        RECT r{x, y, static_cast<LONG>(w) - x / 2, static_cast<LONG>(h)};
        DrawTextW(dc, lines[i].c_str(), static_cast<int>(lines[i].size()), &r,
                  DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        SelectObject(dc, of);
        DeleteObject(f);
        y += static_cast<int>(std::lround((sizes[i] + 7) * s));
    }
    GdiFlush();
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
    BoxSize(sw.extent, &w, &h, &s);
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
