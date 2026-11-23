#!/bin/sh
# Compila build/winmm.dll (Windows x64) con mingw-w64. Uso: ./build.sh [build_de_nms]
set -e
cd "$(dirname "$0")"
NMS_BUILD="${1:-185034}"
python3 tools/gen_headers.py --build "$NMS_BUILD"
CC=x86_64-w64-mingw32-gcc
CXX=x86_64-w64-mingw32-g++
mkdir -p build/obj
for f in buffer hook trampoline; do $CC -O2 -c third_party/src/$f.c -Ithird_party/include -o build/obj/$f.o; done
$CC -O2 -c third_party/src/hde/hde64.c -o build/obj/hde64.o
$CC -c src/winmm_stubs.S -o build/obj/winmm_stubs.o
$CC -c src/fonts.S -o build/obj/fonts.o  # fuentes del recuadro (incluye ../overlay/fonts/*.ttf)
CXXFLAGS="-O2 -std=c++17 -Wall -Wextra -Ithird_party/include -Ithird_party -Ithird_party/vulkan"
for f in dllmain core game_reader http_server vk_probe vk_overlay; do $CXX $CXXFLAGS -c src/$f.cpp -o build/obj/$f.o; done
$CXX -shared -o build/winmm.dll build/obj/*.o src/winmm.def \
    -static -static-libgcc -static-libstdc++ -lversion -lws2_32 -lgdi32 -Wl,--enable-stdcall-fixup -s
echo "OK: build/winmm.dll"
