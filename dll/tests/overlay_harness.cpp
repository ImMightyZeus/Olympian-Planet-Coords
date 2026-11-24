// Prueba del recuadro dentro del juego (vk_overlay.cpp) con un Vulkan simulado, bajo Wine.
// Test of the in-game box (vk_overlay.cpp) against a mock Vulkan, under Wine.
// Comprueba el orden de comandos, los semaforos, la copia y los casos en que no debe tocar nada.
// Olympian Planet Coords — Copyright © 2026 MightyZeus · GPL-3.0 License
#define VK_USE_PLATFORM_WIN32_KHR
#include <windows.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "../src/vk_overlay.hpp"

using namespace olympian;

static int g_fail = 0;
#define CHECK(c)                                                       \
    do {                                                               \
        if (!(c)) {                                                    \
            ++g_fail;                                                  \
            std::printf("FALLA linea %d: %s\n", __LINE__, #c);         \
        }                                                              \
    } while (0)

// ---- Vulkan simulado -------------------------------------------------------------------------------
static int g_dev_obj, g_pd_obj, g_q[3];
static VkDevice DEV = reinterpret_cast<VkDevice>(&g_dev_obj);
static VkPhysicalDevice PD = reinterpret_cast<VkPhysicalDevice>(&g_pd_obj);
static uint64_t g_next_handle = 0x1000;
template <typename T>
T NewHandle() {
    return reinterpret_cast<T>(g_next_handle++);
}

struct Submit {
    std::vector<VkSemaphore> waits;
    std::vector<VkPipelineStageFlags> stages;
    std::vector<VkSemaphore> signals;
    VkFence fence;
    VkCommandBuffer cb;
};
static std::vector<Submit> g_submits;
static std::vector<std::vector<VkSemaphore>> g_present_waits;
static std::vector<std::pair<VkImageLayout, VkImageLayout>> g_barriers;
static std::vector<VkImage> g_barrier_images;
static std::vector<VkBufferImageCopy> g_copies;
static std::vector<VkImage> g_copy_images;
static std::map<VkFence, bool> g_fence_ready;
static std::vector<void*> g_maps;
static bool g_submit_fails = false;
static int g_begins = 0, g_ends = 0, g_resets = 0;
static VkImage g_images[3];
static std::wstring g_text = L"OLYMPIAN PLANET COORDS\n\x01+12.34, -56.78\n» Torre  · 850 m NE";

static void VKAPI_CALL MkGetDeviceQueue(VkDevice, uint32_t fam, uint32_t idx, VkQueue* q) {
    *q = reinterpret_cast<VkQueue>(&g_q[fam == 0 ? idx : 2]);
}
static VkResult VKAPI_CALL MkCreateCommandPool(VkDevice, const VkCommandPoolCreateInfo* ci, const VkAllocationCallbacks*, VkCommandPool* p) {
    CHECK(ci->flags & VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    *p = NewHandle<VkCommandPool>();
    return VK_SUCCESS;
}
static VkResult VKAPI_CALL MkAllocateCommandBuffers(VkDevice, const VkCommandBufferAllocateInfo* ai, VkCommandBuffer* cb) {
    CHECK(ai->commandBufferCount == 1);
    *cb = reinterpret_cast<VkCommandBuffer>(new int(0));
    return VK_SUCCESS;
}
static VkResult VKAPI_CALL MkBeginCommandBuffer(VkCommandBuffer, const VkCommandBufferBeginInfo*) { ++g_begins; return VK_SUCCESS; }
static VkResult VKAPI_CALL MkEndCommandBuffer(VkCommandBuffer) { ++g_ends; return VK_SUCCESS; }
static VkResult VKAPI_CALL MkResetCommandBuffer(VkCommandBuffer, VkCommandBufferResetFlags) { ++g_resets; return VK_SUCCESS; }
static void VKAPI_CALL MkCmdPipelineBarrier(VkCommandBuffer, VkPipelineStageFlags, VkPipelineStageFlags, VkDependencyFlags,
                                          uint32_t, const VkMemoryBarrier*, uint32_t, const VkBufferMemoryBarrier*,
                                          uint32_t n, const VkImageMemoryBarrier* b) {
    CHECK(n == 1);
    g_barriers.push_back({b->oldLayout, b->newLayout});
    g_barrier_images.push_back(b->image);
}
static void VKAPI_CALL MkCmdCopyBufferToImage(VkCommandBuffer, VkBuffer, VkImage img, VkImageLayout l, uint32_t n,
                                            const VkBufferImageCopy* r) {
    CHECK(n == 1 && l == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    g_copies.push_back(*r);
    g_copy_images.push_back(img);
}
static VkResult VKAPI_CALL MkQueueSubmit(VkQueue, uint32_t n, const VkSubmitInfo* s, VkFence f) {
    if (g_submit_fails) return VK_ERROR_DEVICE_LOST;
    CHECK(n == 1 && s->commandBufferCount == 1);
    Submit x;
    x.waits.assign(s->pWaitSemaphores, s->pWaitSemaphores + s->waitSemaphoreCount);
    x.stages.assign(s->pWaitDstStageMask, s->pWaitDstStageMask + s->waitSemaphoreCount);
    x.signals.assign(s->pSignalSemaphores, s->pSignalSemaphores + s->signalSemaphoreCount);
    x.fence = f;
    x.cb = s->pCommandBuffers[0];
    g_submits.push_back(x);
    g_fence_ready[f] = false;
    return VK_SUCCESS;
}
static VkResult VKAPI_CALL MkCreateFence(VkDevice, const VkFenceCreateInfo*, const VkAllocationCallbacks*, VkFence* f) {
    *f = NewHandle<VkFence>();
    g_fence_ready[*f] = false;
    return VK_SUCCESS;
}
static VkResult VKAPI_CALL MkGetFenceStatus(VkDevice, VkFence f) { return g_fence_ready[f] ? VK_SUCCESS : VK_NOT_READY; }
static VkResult VKAPI_CALL MkResetFences(VkDevice, uint32_t n, const VkFence* f) {
    for (uint32_t i = 0; i < n; ++i) g_fence_ready[f[i]] = false;
    return VK_SUCCESS;
}
static VkResult VKAPI_CALL MkCreateSemaphore(VkDevice, const VkSemaphoreCreateInfo*, const VkAllocationCallbacks*, VkSemaphore* s) {
    *s = NewHandle<VkSemaphore>();
    return VK_SUCCESS;
}
static VkResult VKAPI_CALL MkCreateBuffer(VkDevice, const VkBufferCreateInfo* ci, const VkAllocationCallbacks*, VkBuffer* b) {
    CHECK(ci->usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    *b = NewHandle<VkBuffer>();
    return VK_SUCCESS;
}
static void VKAPI_CALL MkGetBufferMemoryRequirements(VkDevice, VkBuffer, VkMemoryRequirements* r) {
    r->size = 1024 * 320 * 4;
    r->alignment = 256;
    r->memoryTypeBits = 0b110;  // tipos 1 y 2
}
static VkResult VKAPI_CALL MkAllocateMemory(VkDevice, const VkMemoryAllocateInfo* ai, const VkAllocationCallbacks*, VkDeviceMemory* m) {
    CHECK(ai->memoryTypeIndex == 2);  // el 1 no es visible por la CPU
    *m = NewHandle<VkDeviceMemory>();
    return VK_SUCCESS;
}
static VkResult VKAPI_CALL MkBindBufferMemory(VkDevice, VkBuffer, VkDeviceMemory, VkDeviceSize) { return VK_SUCCESS; }
static VkResult VKAPI_CALL MkMapMemory(VkDevice, VkDeviceMemory, VkDeviceSize, VkDeviceSize, VkMemoryMapFlags, void** p) {
    *p = std::calloc(1024 * 320 * 4, 1);
    g_maps.push_back(*p);
    return VK_SUCCESS;
}
static VkResult VKAPI_CALL MkGetSwapchainImagesKHR(VkDevice, VkSwapchainKHR, uint32_t* n, VkImage* imgs) {
    if (!imgs) {
        *n = 3;
        return VK_SUCCESS;
    }
    for (uint32_t i = 0; i < *n && i < 3; ++i) imgs[i] = g_images[i];
    return VK_SUCCESS;
}
static void VKAPI_CALL MemProps(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties* p) {
    std::memset(p, 0, sizeof(*p));
    p->memoryTypeCount = 3;
    p->memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    p->memoryTypes[1].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    p->memoryTypes[2].propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
}
static PFN_vkVoidFunction VKAPI_CALL Gdpa(VkDevice, const char* n) {
#define F(name) if (!std::strcmp(n, "vk" #name)) return reinterpret_cast<PFN_vkVoidFunction>(&Mk##name);
    F(GetDeviceQueue) F(CreateCommandPool) F(AllocateCommandBuffers) F(BeginCommandBuffer) F(EndCommandBuffer)
    F(ResetCommandBuffer) F(CmdPipelineBarrier) F(CmdCopyBufferToImage) F(QueueSubmit) F(CreateFence)
    F(GetFenceStatus) F(ResetFences) F(CreateSemaphore) F(CreateBuffer) F(GetBufferMemoryRequirements)
    F(AllocateMemory) F(BindBufferMemory) F(MapMemory) F(GetSwapchainImagesKHR)
#undef F
    return nullptr;
}
static VkResult VKAPI_CALL RealPresent(VkQueue, const VkPresentInfoKHR* pi) {
    g_present_waits.emplace_back(pi->pWaitSemaphores, pi->pWaitSemaphores + pi->waitSemaphoreCount);
    return VK_SUCCESS;
}
static void Log(const char* fmt, ...) {
    va_list a;
    va_start(a, fmt);
    std::vprintf(fmt, a);
    va_end(a);
    std::printf("\n");
}
static std::wstring Text() { return g_text; }

// OVERLAY_TEXT (UTF-8) cambia el texto de la primera prueba para mirar el recuadro con OVERLAY_DUMP:
// '|' separa lineas y '^' al inicio de una linea la marca como coordenadas.
static void TextFromEnv() {
    // OVERLAY_TEXT_FILE: texto UTF-8 tal cual (con los codigos de estilo del panel)
    if (const wchar_t* path = _wgetenv(L"OVERLAY_TEXT_FILE")) {
        if (FILE* f = _wfopen(path, L"rb")) {
            std::string u;
            char buf[4096];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), f)) > 0) u.append(buf, n);
            fclose(f);
            int m = MultiByteToWideChar(CP_UTF8, 0, u.data(), static_cast<int>(u.size()), nullptr, 0);
            std::wstring w(static_cast<size_t>(m), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, u.data(), static_cast<int>(u.size()), &w[0], m);
            g_text = w;
            return;
        }
    }
    const wchar_t* t = _wgetenv(L"OVERLAY_TEXT");
    if (!t) return;
    std::wstring w(t);
    for (auto& c : w) {
        if (c == L'|') c = L'\n';
        if (c == L'^') c = kNumericLine;
    }
    g_text = w;
}

static VkResult Present(VkQueue q, VkSwapchainKHR sc, uint32_t idx, VkSemaphore wait) {
    VkPresentInfoKHR pi{};
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &wait;
    pi.swapchainCount = 1;
    pi.pSwapchains = &sc;
    pi.pImageIndices = &idx;
    return OverlayPresent(q, &pi, &RealPresent);
}

static uint32_t Pixel(const uint8_t* map, uint32_t w, uint32_t x, uint32_t y) {
    uint32_t v;
    std::memcpy(&v, map + (static_cast<size_t>(y) * w + x) * 4, 4);
    return v;
}

int main() {
    for (auto& i : g_images) i = NewHandle<VkImage>();
    TextFromEnv();
    OverlaySetup(&Log, &Text, &MemProps);

    VkDeviceQueueCreateInfo qci[2]{};
    qci[0].sType = qci[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci[0].queueFamilyIndex = 0;
    qci[0].queueCount = 2;
    qci[1].queueFamilyIndex = 2;
    qci[1].queueCount = 1;
    VkDeviceCreateInfo dci{};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = 2;
    dci.pQueueCreateInfos = qci;
    OverlayOnDevice(PD, DEV, &dci, &Gdpa);
    VkQueue q0 = reinterpret_cast<VkQueue>(&g_q[0]);
    VkQueue unknown_q = reinterpret_cast<VkQueue>(&g_fail);

    VkSwapchainKHR sc = NewHandle<VkSwapchainKHR>();
    VkSwapchainCreateInfoKHR sci{};
    sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    sci.imageExtent = {1920, 1200};
    sci.imageUsage = 0x1e;  // como el juego
    OverlayOnSwapchain(DEV, &sci, sc);

    // 1. Una presentacion normal: espera al juego, copia, senala lo suyo y presenta esperando eso.
    VkSemaphore game = NewHandle<VkSemaphore>();
    CHECK(Present(q0, sc, 1, game) == VK_SUCCESS);
    CHECK(g_submits.size() == 1);
    CHECK(g_submits[0].waits.size() == 1 && g_submits[0].waits[0] == game);
    CHECK(g_submits[0].stages[0] == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    CHECK(g_submits[0].signals.size() == 1 && g_submits[0].signals[0] != game);
    CHECK(g_present_waits.size() == 1 && g_present_waits[0].size() == 1 && g_present_waits[0][0] == g_submits[0].signals[0]);
    CHECK(g_barriers.size() == 2);
    CHECK(g_barriers[0].first == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR && g_barriers[0].second == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    CHECK(g_barriers[1].first == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && g_barriers[1].second == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    CHECK(g_barrier_images[0] == g_images[1] && g_copy_images[0] == g_images[1]);
    CHECK(g_copies.size() == 1);
    const VkBufferImageCopy& c = g_copies[0];
    const uint32_t first_w = c.imageExtent.width, first_h = c.imageExtent.height;  // (c deja de valer luego)
    const int32_t first_off = c.imageOffset.x;
    // Tamano por el DPI de Windows (como el .exe), no por la resolucion. El ancho se ajusta al texto.
    CHECK(c.imageExtent.width >= 120 && c.imageExtent.width <= 400 && c.imageExtent.height >= 40 &&
          c.imageExtent.height <= 140 && c.imageExtent.depth == 1);
    std::printf("recuadro a 1200 de alto: %ux%u\n", c.imageExtent.width, c.imageExtent.height);
    CHECK(c.imageOffset.x == c.imageOffset.y && c.imageOffset.x >= 18 && c.imageOffset.x <= 60);
    CHECK(c.bufferRowLength == c.imageExtent.width && c.bufferImageHeight == c.imageExtent.height);
    CHECK(g_begins == 1 && g_ends == 1);
    std::printf("copia: %ux%u en (%d,%d)\n", c.imageExtent.width, c.imageExtent.height, c.imageOffset.x, c.imageOffset.y);

    // Pixeles: barra de acento a la izquierda, borde, fondo y texto claro (R8G8B8A8: bytes R,G,B,A).
    const uint8_t* map = static_cast<const uint8_t*>(g_maps[0]);
    uint32_t w = c.imageExtent.width, h = c.imageExtent.height;  // 444 x 107 con el texto de la prueba
    CHECK(Pixel(map, w, 1, h / 2) == 0xFF3DA3E8);    // acento (232,163,61)
    CHECK(Pixel(map, w, w - 1, h / 2) == 0xFF48403A);  // borde (58,64,72)
    CHECK(Pixel(map, w, w - 3, h - 3) == 0xFF1E1814);  // fondo (20,24,30)
    int bright = 0, accent_text = 0;
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 20; x < w; ++x) {
            uint32_t p = Pixel(map, w, x, y);
            int r = p & 0xFF, g = (p >> 8) & 0xFF, b = (p >> 16) & 0xFF;
            if (r > 200 && g > 200 && b > 200) ++bright;            // linea de coordenadas (blanca)
            if (r > 180 && g > 110 && g < 190 && b < 110) ++accent_text;  // titulo (naranja)
            CHECK((p >> 24) == 0xFF);
        }
    std::printf("pixeles de texto: blancos=%d naranjas=%d\n", bright, accent_text);
    if (const char* dump = std::getenv("OVERLAY_DUMP")) {  // para mirar el recuadro: RGBA crudo
        if (FILE* f = std::fopen(dump, "wb")) {
            std::fwrite(map, 4, static_cast<size_t>(w) * h, f);
            std::fclose(f);
        }
        std::string dims = std::string(dump) + ".txt";
        if (FILE* f = std::fopen(dims.c_str(), "w")) {
            std::fprintf(f, "%u %u\n", w, h);
            std::fclose(f);
        }
    }
    CHECK(bright > 80 && accent_text > 60);  // letra de 8 y 6 puntos (como el .exe) a 96 DPI

    // 2. La GPU no termina: tras 4 cuadros en vuelo, presenta sin tocar nada (no espera).
    for (int i = 0; i < 3; ++i) Present(q0, sc, (i + 2) % 3, game);
    CHECK(g_submits.size() == 4);
    size_t before = g_present_waits.size();
    Present(q0, sc, 0, game);
    CHECK(g_submits.size() == 4);
    CHECK(g_present_waits.size() == before + 1 && g_present_waits.back()[0] == game);
    // 3. La GPU termina: vuelve a dibujar con el primer cuadro.
    g_fence_ready[g_submits[0].fence] = true;
    Present(q0, sc, 2, game);
    CHECK(g_submits.size() == 5 && g_submits[4].fence == g_submits[0].fence && g_submits[4].cb == g_submits[0].cb);
    // Mismo indice de imagen (2) -> mismo semaforo (se reutiliza cuando el juego vuelve a usar esa imagen).
    CHECK(g_submits[4].signals[0] == g_submits[1].signals[0]);
    CHECK(g_submits[2].signals[0] != g_submits[1].signals[0] && g_submits[3].signals[0] == g_submits[0].signals[0]);

    // 4. Casos en que no toca nada.
    for (auto& f : g_fence_ready) f.second = true;
    size_t subs = g_submits.size();
    Present(unknown_q, sc, 0, game);  // cola desconocida
    CHECK(g_submits.size() == subs && g_present_waits.back()[0] == game);
    Present(q0, NewHandle<VkSwapchainKHR>(), 0, game);  // cadena desconocida
    CHECK(g_submits.size() == subs && g_present_waits.back()[0] == game);
    Present(q0, sc, 7, game);  // indice fuera de rango
    CHECK(g_submits.size() == subs && g_present_waits.back()[0] == game);
    VkSwapchainKHR hdr = NewHandle<VkSwapchainKHR>();
    sci.imageFormat = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    OverlayOnSwapchain(DEV, &sci, hdr);
    Present(q0, hdr, 0, game);  // formato sin soporte
    CHECK(g_submits.size() == subs && g_present_waits.back()[0] == game);
    g_text = L"";
    Sleep(150);
    Present(q0, sc, 0, game);  // apagado ("ingame_box": false)
    CHECK(g_submits.size() == subs && g_present_waits.back()[0] == game);

    // 5. Formato BGRA: mismo recuadro con los canales en otro orden.
    g_text = L"OLYMPIAN PLANET COORDS\n+1.00, +2.00\n";
    Sleep(150);
    VkSwapchainKHR bgra = NewHandle<VkSwapchainKHR>();
    sci.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    sci.imageExtent = {2560, 1440};
    OverlayOnSwapchain(DEV, &sci, bgra);
    Present(q0, bgra, 1, game);
    CHECK(g_submits.size() == subs + 1);
    const VkBufferImageCopy& c2 = g_copies.back();
    // 1440 de alto: mismo tamano que a 1200 (depende del DPI, no de la resolucion)
    const uint32_t w1 = first_w, h1 = first_h, w2 = c2.imageExtent.width;
    std::printf("1440: %ux%u, 1200: %ux%u\n", w2, c2.imageExtent.height, w1, h1);
    CHECK(c2.imageExtent.height == h1 && w2 > 60);  // otro texto (ancho distinto), mismo alto
    CHECK(c2.imageOffset.x == first_off);
    bool found = false;
    for (void* m : g_maps)
        if (Pixel(static_cast<uint8_t*>(m), w2, 1, h1 / 2) == 0xFFE8A33D) found = true;  // acento en BGRA
    CHECK(found);
    // Cadena destruida: ya no se dibuja en ella.
    OverlayOnDestroySwapchain(bgra);
    subs = g_submits.size();
    Present(q0, bgra, 1, game);
    CHECK(g_submits.size() == subs);

    // 6. Un envio que falla: el recuadro se apaga y el juego presenta con sus propios semaforos.
    g_submit_fails = true;
    before = g_present_waits.size();
    CHECK(Present(q0, sc, 1, game) == VK_SUCCESS);
    CHECK(g_present_waits.size() == before + 1 && g_present_waits.back()[0] == game);
    g_submit_fails = false;
    Present(q0, sc, 2, game);
    CHECK(g_submits.size() == subs && g_present_waits.back()[0] == game);
    std::string js = OverlayJson();
    std::printf("%s\n", js.c_str());
    CHECK(js.find("\"broken\":true") != std::string::npos && js.find("vkQueueSubmit -> -4") != std::string::npos);

    std::printf(g_fail ? "FALLOS=%d\n" : "TODO OK\n", g_fail);
    return g_fail ? 1 : 0;
}
