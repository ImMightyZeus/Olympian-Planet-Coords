// Olympian Planet Coords (DLL) - recuadro dentro del juego (Vulkan) / in-game box (Vulkan)
// Olympian Planet Coords — Copyright © 2026 MightyZeus · Licencia GPL-3.0
#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <windows.h>

#include <string>

#include <vulkan/vulkan.h>

namespace olympian {

using OverlayLog = void (*)(const char* fmt, ...);
// Texto del recuadro: lineas separadas por '\n' (UTF-16). Vacio = no dibujar. Una linea que empieza
// con kNumericLine son coordenadas: los digitos se dibujan en DSEG7 (el marcador no se dibuja).
constexpr wchar_t kNumericLine = L'\x01';
using OverlayTextFn = std::wstring (*)();

void OverlaySetup(OverlayLog log, OverlayTextFn text, PFN_vkGetPhysicalDeviceMemoryProperties mem_props);
// Despues de vkCreateDevice del juego. gdpa = vkGetDeviceProcAddr real (sin envolver).
void OverlayOnDevice(VkPhysicalDevice pd, VkDevice dev, const VkDeviceCreateInfo* ci, PFN_vkGetDeviceProcAddr gdpa);
void OverlayOnSwapchain(VkDevice dev, const VkSwapchainCreateInfoKHR* ci, VkSwapchainKHR sc);
void OverlayOnDestroySwapchain(VkSwapchainKHR sc);
// Envuelve vkQueuePresentKHR: copia el recuadro en la imagen que se va a mostrar y luego presenta.
// Si algo no cuadra (otra cadena, formato raro, recursos ocupados) presenta sin tocar nada.
VkResult OverlayPresent(VkQueue q, const VkPresentInfoKHR* pi, PFN_vkQueuePresentKHR real);
std::string OverlayJson();

}  // namespace olympian
