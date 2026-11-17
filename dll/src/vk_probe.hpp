// Olympian Planet Coords (DLL) - sonda de Vulkan (solo diagnostico) / Vulkan probe (diagnostics only)
// Olympian Planet Coords — Copyright © 2026 MightyZeus · Licencia GPL-3.0
#pragma once

#include <windows.h>

#include <string>

namespace olympian {

using VkProbeLog = void (*)(const char* fmt, ...);

// Engancha vkCreateInstance, vkCreateDevice, vkGetDeviceProcAddr, vkCreateSwapchainKHR y
// vkQueuePresentKHR del cargador de Vulkan. Solo anota; todo pasa sin cambios al juego.
bool VkProbeInstall(VkProbeLog log);
// Escribe en el log un resumen (GPU, cadena de imagenes, presentaciones, ventana, modulos cargados).
void VkProbeReport(const char* when, HWND game_window);
// Resumen para /debug.
std::string VkProbeJson();

}  // namespace olympian
