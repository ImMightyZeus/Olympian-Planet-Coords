// Olympian Planet Coords (DLL) - sonda de Vulkan (solo diagnostico, no dibuja nada)
// Olympian Planet Coords (DLL) - Vulkan probe (diagnostics only, draws nothing)
//
// Primer paso hacia el recuadro DENTRO del juego (sin .exe y sin modo Sin bordes): antes de dibujar,
// hay que saber por donde pasa el juego para presentar cada imagen. Esta sonda engancha las funciones
// del cargador de Vulkan (vulkan-1.dll) que el juego usa, las deja pasar sin cambiar nada y anota en
// olympian.log:
//   - instancia y dispositivo (GPU, capas y extensiones que pide el juego),
//   - la cadena de imagenes (tamano, formato, modo de presentacion, pantalla completa exclusiva),
//   - cuantas imagenes por segundo presenta y por que camino (importacion o vkGetDeviceProcAddr),
//   - que otros programas que dibujan sobre el juego estan cargados (Steam, RivaTuner, ReShade, DLSS...),
//   - si la ventana del juego es pantalla completa o Sin bordes.
//
// Olympian Planet Coords — Copyright © 2026 MightyZeus · Licencia GPL-3.0

#define VK_USE_PLATFORM_WIN32_KHR
#include "vk_probe.hpp"

#include <windows.h>
#include <tlhelp32.h>

#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <set>
#include <string>

#include <vulkan/vulkan.h>

#include "MinHook.h"

namespace olympian {
namespace {

VkProbeLog g_log = nullptr;
std::mutex g_mu;

PFN_vkCreateInstance o_create_instance = nullptr;
PFN_vkCreateDevice o_create_device = nullptr;
PFN_vkGetDeviceProcAddr o_get_device_proc = nullptr;
PFN_vkCreateSwapchainKHR o_create_swapchain_export = nullptr;
PFN_vkQueuePresentKHR o_present_export = nullptr;
PFN_vkGetPhysicalDeviceProperties p_get_props = nullptr;

// Punteros reales que el juego pidio por vkGetDeviceProcAddr (los envolvemos para contarlos).
PFN_vkCreateSwapchainKHR r_create_swapchain_dev = nullptr;
PFN_vkQueuePresentKHR r_present_dev = nullptr;

std::atomic<unsigned long long> g_present_export{0}, g_present_dev{0}, g_swapchains{0};
std::atomic<bool> g_saw_instance{false}, g_saw_device{false};
std::atomic<unsigned long long> g_last_present_ms{0};
std::set<std::string> g_requested;
std::set<uintptr_t> g_queues;
VkExtent2D g_extent{0, 0};
VkFormat g_format = VK_FORMAT_UNDEFINED;
VkPresentModeKHR g_mode = VK_PRESENT_MODE_MAX_ENUM_KHR;
uint32_t g_images = 0;
std::string g_gpu;

const char* ModeName(VkPresentModeKHR m) {
    switch (m) {
        case VK_PRESENT_MODE_IMMEDIATE_KHR: return "IMMEDIATE";
        case VK_PRESENT_MODE_MAILBOX_KHR: return "MAILBOX";
        case VK_PRESENT_MODE_FIFO_KHR: return "FIFO (vsync)";
        case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return "FIFO_RELAXED";
        default: return "otro";
    }
}

std::string Join(const char* const* names, uint32_t n) {
    std::string s;
    for (uint32_t i = 0; i < n; ++i) {
        if (i) s += ", ";
        s += names[i] ? names[i] : "?";
    }
    return s.empty() ? "(ninguna)" : s;
}

// Cadena pNext: anota los sType (por ejemplo VK_EXT_full_screen_exclusive = 1000255000).
std::string Chain(const void* p) {
    std::string s;
    for (auto* b = static_cast<const VkBaseInStructure*>(p); b; b = b->pNext) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%s%d", s.empty() ? "" : ",", static_cast<int>(b->sType));
        s += buf;
    }
    return s.empty() ? "-" : s;
}

VkResult VKAPI_CALL HkCreateInstance(const VkInstanceCreateInfo* ci, const VkAllocationCallbacks* a,
                                     VkInstance* out) {
    VkResult r = o_create_instance(ci, a, out);
    g_saw_instance = true;
    if (g_log && ci) {
        const VkApplicationInfo* ai = ci->pApplicationInfo;
        g_log("[vk] vkCreateInstance -> %d  app=%s engine=%s api=%u.%u", static_cast<int>(r),
              ai && ai->pApplicationName ? ai->pApplicationName : "-",
              ai && ai->pEngineName ? ai->pEngineName : "-", ai ? VK_API_VERSION_MAJOR(ai->apiVersion) : 0,
              ai ? VK_API_VERSION_MINOR(ai->apiVersion) : 0);
        g_log("[vk]   capas: %s", Join(ci->ppEnabledLayerNames, ci->enabledLayerCount).c_str());
        g_log("[vk]   extensiones: %s", Join(ci->ppEnabledExtensionNames, ci->enabledExtensionCount).c_str());
    }
    return r;
}

VkResult VKAPI_CALL HkCreateDevice(VkPhysicalDevice pd, const VkDeviceCreateInfo* ci, const VkAllocationCallbacks* a,
                                   VkDevice* out) {
    VkResult r = o_create_device(pd, ci, a, out);
    g_saw_device = true;
    if (g_log && ci) {
        VkPhysicalDeviceProperties props{};
        if (p_get_props) p_get_props(pd, &props);
        {
            std::lock_guard<std::mutex> g(g_mu);
            g_gpu = props.deviceName;
        }
        g_log("[vk] vkCreateDevice -> %d  GPU=\"%s\" tipo=%d driver=0x%x api=%u.%u", static_cast<int>(r),
              props.deviceName, static_cast<int>(props.deviceType), props.driverVersion,
              VK_API_VERSION_MAJOR(props.apiVersion), VK_API_VERSION_MINOR(props.apiVersion));
        for (uint32_t i = 0; i < ci->queueCreateInfoCount; ++i)
            g_log("[vk]   cola: familia=%u cantidad=%u", ci->pQueueCreateInfos[i].queueFamilyIndex,
                  ci->pQueueCreateInfos[i].queueCount);
        g_log("[vk]   extensiones: %s", Join(ci->ppEnabledExtensionNames, ci->enabledExtensionCount).c_str());
    }
    return r;
}

void NoteSwapchain(const VkSwapchainCreateInfoKHR* ci, VkResult r, const char* path) {
    ++g_swapchains;
    if (!ci) return;
    {
        std::lock_guard<std::mutex> g(g_mu);
        g_extent = ci->imageExtent;
        g_format = ci->imageFormat;
        g_mode = ci->presentMode;
        g_images = ci->minImageCount;
    }
    if (g_log)
        g_log("[vk] vkCreateSwapchainKHR (%s) -> %d  %ux%u formato=%d espacio=%d imagenes>=%u uso=0x%x modo=%s "
              "alpha=0x%x vieja=%s pNext=[%s]",
              path, static_cast<int>(r), ci->imageExtent.width, ci->imageExtent.height,
              static_cast<int>(ci->imageFormat), static_cast<int>(ci->imageColorSpace), ci->minImageCount,
              ci->imageUsage, ModeName(ci->presentMode), ci->compositeAlpha, ci->oldSwapchain ? "si" : "no",
              Chain(ci->pNext).c_str());
}

VkResult VKAPI_CALL HkCreateSwapchainExport(VkDevice d, const VkSwapchainCreateInfoKHR* ci,
                                            const VkAllocationCallbacks* a, VkSwapchainKHR* out) {
    VkResult r = o_create_swapchain_export(d, ci, a, out);
    NoteSwapchain(ci, r, "exportacion");
    return r;
}

VkResult VKAPI_CALL HkCreateSwapchainDev(VkDevice d, const VkSwapchainCreateInfoKHR* ci,
                                         const VkAllocationCallbacks* a, VkSwapchainKHR* out) {
    VkResult r = r_create_swapchain_dev(d, ci, a, out);
    NoteSwapchain(ci, r, "GetDeviceProcAddr");
    return r;
}

void NotePresent(VkQueue q, const VkPresentInfoKHR* pi, std::atomic<unsigned long long>& counter) {
    ++counter;
    g_last_present_ms = GetTickCount64();
    if (counter.load() <= 3 && g_log && pi)
        g_log("[vk] vkQueuePresentKHR #%llu cola=%p cadenas=%u esperas=%u pNext=[%s]", counter.load(),
              static_cast<void*>(q), pi->swapchainCount, pi->waitSemaphoreCount, Chain(pi->pNext).c_str());
    std::lock_guard<std::mutex> g(g_mu);
    if (g_queues.size() < 8) g_queues.insert(reinterpret_cast<uintptr_t>(q));
}

VkResult VKAPI_CALL HkPresentExport(VkQueue q, const VkPresentInfoKHR* pi) {
    NotePresent(q, pi, g_present_export);
    return o_present_export(q, pi);
}

VkResult VKAPI_CALL HkPresentDev(VkQueue q, const VkPresentInfoKHR* pi) {
    NotePresent(q, pi, g_present_dev);
    return r_present_dev(q, pi);
}

PFN_vkVoidFunction VKAPI_CALL HkGetDeviceProcAddr(VkDevice d, const char* name) {
    PFN_vkVoidFunction f = o_get_device_proc(d, name);
    if (!name || !f) return f;
    bool interesting = std::strstr(name, "Present") || std::strstr(name, "Swapchain") ||
                       std::strstr(name, "AcquireNextImage") || std::strstr(name, "QueueSubmit");
    if (interesting) {
        bool first;
        {
            std::lock_guard<std::mutex> g(g_mu);
            first = g_requested.insert(name).second;
        }
        if (first && g_log) g_log("[vk] el juego pide por vkGetDeviceProcAddr: %s", name);
    }
    if (std::strcmp(name, "vkCreateSwapchainKHR") == 0) {
        r_create_swapchain_dev = reinterpret_cast<PFN_vkCreateSwapchainKHR>(f);
        return reinterpret_cast<PFN_vkVoidFunction>(&HkCreateSwapchainDev);
    }
    if (std::strcmp(name, "vkQueuePresentKHR") == 0) {
        r_present_dev = reinterpret_cast<PFN_vkQueuePresentKHR>(f);
        return reinterpret_cast<PFN_vkVoidFunction>(&HkPresentDev);
    }
    return f;
}

bool Hook(HMODULE vk, const char* name, void* detour, void** original) {
    void* target = reinterpret_cast<void*>(GetProcAddress(vk, name));
    if (!target) {
        if (g_log) g_log("[vk] %s no esta en vulkan-1.dll", name);
        return false;
    }
    MH_STATUS s = MH_CreateHook(target, detour, original);
    if (s == MH_OK) s = MH_EnableHook(target);
    if (g_log) g_log("[vk] gancho %s: %s", name, MH_StatusToString(s));
    return s == MH_OK;
}

std::string Lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

}  // namespace

bool VkProbeInstall(VkProbeLog log) {
    g_log = log;
    HMODULE vk = GetModuleHandleW(L"vulkan-1.dll");
    if (!vk) vk = LoadLibraryW(L"vulkan-1.dll");  // el juego lo importa: ya deberia estar cargado
    if (!vk) {
        if (g_log) g_log("[vk] vulkan-1.dll no esta cargado: sonda desactivada");
        return false;
    }
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(vk, path, MAX_PATH);
    if (g_log) g_log("[vk] cargador: %ls", path);
    MH_STATUS init = MH_Initialize();
    if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) {
        if (g_log) g_log("[vk] MinHook: %s", MH_StatusToString(init));
        return false;
    }
    p_get_props = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
        reinterpret_cast<void*>(GetProcAddress(vk, "vkGetPhysicalDeviceProperties")));
    Hook(vk, "vkCreateInstance", reinterpret_cast<void*>(&HkCreateInstance), reinterpret_cast<void**>(&o_create_instance));
    Hook(vk, "vkCreateDevice", reinterpret_cast<void*>(&HkCreateDevice), reinterpret_cast<void**>(&o_create_device));
    Hook(vk, "vkGetDeviceProcAddr", reinterpret_cast<void*>(&HkGetDeviceProcAddr),
         reinterpret_cast<void**>(&o_get_device_proc));
    Hook(vk, "vkCreateSwapchainKHR", reinterpret_cast<void*>(&HkCreateSwapchainExport),
         reinterpret_cast<void**>(&o_create_swapchain_export));
    Hook(vk, "vkQueuePresentKHR", reinterpret_cast<void*>(&HkPresentExport),
         reinterpret_cast<void**>(&o_present_export));
    return true;
}

void VkProbeReport(const char* when, HWND game_window) {
    if (!g_log) return;
    std::string gpu, queues;
    VkExtent2D ext;
    VkFormat fmt;
    VkPresentModeKHR mode;
    uint32_t imgs;
    {
        std::lock_guard<std::mutex> g(g_mu);
        gpu = g_gpu;
        ext = g_extent;
        fmt = g_format;
        mode = g_mode;
        imgs = g_images;
        queues = std::to_string(g_queues.size());
    }
    g_log("[vk] --- informe %s: instancia=%s dispositivo=%s GPU=\"%s\" cadenas_creadas=%llu  %ux%u formato=%d "
          "modo=%s imagenes>=%u  presentaciones: exportacion=%llu GetDeviceProcAddr=%llu  colas distintas=%s",
          when, g_saw_instance ? "vista" : "NO vista (se creo antes del gancho)", g_saw_device ? "visto" : "NO visto",
          gpu.c_str(), g_swapchains.load(), ext.width, ext.height, static_cast<int>(fmt),
          mode == VK_PRESENT_MODE_MAX_ENUM_KHR ? "-" : ModeName(mode), imgs, g_present_export.load(),
          g_present_dev.load(), queues.c_str());

    // Ventana del juego: pantalla completa / Sin bordes / ventana.
    if (game_window) {
        LONG style = GetWindowLongW(game_window, GWL_STYLE);
        LONG ex = GetWindowLongW(game_window, GWL_EXSTYLE);
        RECT wr{}, cr{};
        GetWindowRect(game_window, &wr);
        GetClientRect(game_window, &cr);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        GetMonitorInfoW(MonitorFromWindow(game_window, MONITOR_DEFAULTTONEAREST), &mi);
        bool covers = wr.left <= mi.rcMonitor.left && wr.top <= mi.rcMonitor.top && wr.right >= mi.rcMonitor.right &&
                      wr.bottom >= mi.rcMonitor.bottom;
        g_log("[vk] ventana: estilo=0x%08lx ext=0x%08lx %s%s  ventana=%ldx%ld cliente=%ldx%ld monitor=%ldx%ld %s",
              static_cast<unsigned long>(style), static_cast<unsigned long>(ex),
              (style & WS_POPUP) ? "POPUP " : "", (style & WS_CAPTION) == WS_CAPTION ? "CON_BORDES" : "SIN_BORDES",
              wr.right - wr.left, wr.bottom - wr.top, cr.right, cr.bottom, mi.rcMonitor.right - mi.rcMonitor.left,
              mi.rcMonitor.bottom - mi.rcMonitor.top, covers ? "cubre el monitor" : "no cubre el monitor");
    }

    // Programas cargados en el juego que tambien dibujan o interceptan la presentacion.
    static const char* kKnown[] = {"overlay", "layer", "reshade", "rtss", "sl.", "dlss", "nvngx", "xess", "ffx",
                                   "fsr", "discord", "obs", "afterburner", "fraps", "nvidia", "amd", "igo",
                                   "medal", "overwolf", "xinput9_1_0", "vulkan"};
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (snap == INVALID_HANDLE_VALUE) return;
    MODULEENTRY32W me{};
    me.dwSize = sizeof(me);
    std::string found;
    for (BOOL ok = Module32FirstW(snap, &me); ok; ok = Module32NextW(snap, &me)) {
        char name[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, me.szModule, -1, name, sizeof(name), nullptr, nullptr);
        std::string low = Lower(name);
        for (const char* k : kKnown)
            if (low.find(k) != std::string::npos) {
                found += found.empty() ? "" : ", ";
                found += name;
                break;
            }
    }
    CloseHandle(snap);
    g_log("[vk] modulos relevantes: %s", found.empty() ? "(ninguno)" : found.c_str());
}

std::string VkProbeJson() {
    std::lock_guard<std::mutex> g(g_mu);
    char buf[512];
    snprintf(buf, sizeof(buf),
             "{\"instance_seen\":%s,\"device_seen\":%s,\"gpu\":\"%s\",\"swapchains\":%llu,\"extent\":[%u,%u],"
             "\"format\":%d,\"present_mode\":%d,\"present_export\":%llu,\"present_getdeviceprocaddr\":%llu,"
             "\"queues\":%zu}",
             g_saw_instance ? "true" : "false", g_saw_device ? "true" : "false", g_gpu.c_str(), g_swapchains.load(),
             g_extent.width, g_extent.height, static_cast<int>(g_format), static_cast<int>(g_mode),
             g_present_export.load(), g_present_dev.load(), g_queues.size());
    return buf;
}

}  // namespace olympian
