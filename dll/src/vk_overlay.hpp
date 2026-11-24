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
// Codigo de estilo al principio de la linea (antes de kNumericLine). Si una linea lo lleva, el texto es
// el panel: recuadro mas ancho, hasta 24 lineas y alto segun las lineas.
constexpr wchar_t kStyleTitle = L'\x10';     // titulo (naranja, negrita)
constexpr wchar_t kStyleBig = L'\x11';       // grande (coordenadas o lugar)
constexpr wchar_t kStyleSmall = L'\x12';     // pequena gris (ayudas)
constexpr wchar_t kStyleItem = L'\x13';      // fila de una lista
constexpr wchar_t kStyleSelected = L'\x14';  // fila elegida (fondo marcado)
constexpr wchar_t kStyleError = L'\x15';     // aviso de error (rojo)
constexpr wchar_t kStyleLabel = L'\x16';     // etiqueta (naranja)
constexpr wchar_t kStyleInput = L'\x17';     // casilla de texto
constexpr wchar_t kStyleOk = L'\x18';        // aviso correcto (verde)
constexpr wchar_t kStyleDest = L'\x19';      // coordenadas del destino (azul)
// Lineas de control (no se dibujan): kBoxBig + 'G' = tamano Grande; kBoxArrow + "yaw;pitch;morph" =
// flecha 3D hacia el destino a la derecha (morph 1 = esfera en reposo, sin destino).
constexpr wchar_t kBoxArrow = L'\x1A';
constexpr wchar_t kBoxBig = L'\x1B';
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
