"""
Olympian Planet Coords v0.1.0-beta - overlay de esquina superior izquierda

Olympian Planet Coords — Copyright © 2026 MightyZeus · Licencia GPL-3.0
Apoya el proyecto: https://paypal.me/ImMightyZeus · Repositorio: https://github.com/ImMightyZeus/Olympian-Planet-Coords

Agradecimientos:
A mi querida esposa y mi amada familia, por permitirme seguir siendo un niño aun en mi adultez.
Pangro Daer Barun D'Gross, por seguirme corrompiendo con videojuegos.
Fenrir, porque aunque ya casi no juega, aún se presta a conversar sobre mis aventuras y desventuras.

Software libre bajo la GNU General Public License v3 (archivo LICENSE). Proyecto hecho por fans, sin
relacion con Hello Games. Se desarrolla con la ayuda de una IA (Claude); la direccion del proyecto y
las pruebas en el juego real son trabajo humano. Textos en espanol e ingles (menu "Idioma / Language").

Ventana pequena, sin bordes y siempre encima del juego que:
  * muestra la latitud/longitud actual (AHORA) y, si hay destino en este planeta, el DESTINO
    con distancia y rumbo;
  * a la derecha, una flecha 3D ambar dentro de un circulo dorado que gira e inclina hacia el
    destino; sin destino (o al llegar) se convierte en una esfera y todo se reduce al 40%;
  * al hacer CLIC (o pulsar la tecla elegida dentro del juego, F9 por defecto) se convierte en un
    campo de texto;
    ENTER = poner destino, ESC = cancelar, flechas ARRIBA/ABAJO = historial (10);
  * clic derecho = menu (tamano, tecla, idioma, quitar destino, acerca de, mover, salir).

La flecha se dibuja con Pillow a 4x y se reduce (bordes suaves), y se muestra en una ventana propia
con transparencia real por pixel (WS_EX_LAYERED + UpdateLayeredWindow). Sin Pillow, o fuera de
Windows, se usa el dibujo anterior en un Canvas (bordes duros). Las fuentes (DSEG7 Classic y Share Tech Mono, licencia
SIL OFL 1.1) se cargan en privado para este programa: no se instalan en Windows ni piden
permisos de administrador. Requiere NMS en modo "Borderless" (sin bordes).

Uso:  py olympian_overlay.py   [--port 5000] [--x 12] [--y 12]
"""

from __future__ import annotations

import argparse
import json
import math
import os
import queue
import re
import sys
import threading
import time
import tkinter as tk
import tkinter.font as tkfont
import urllib.parse
import urllib.request
import webbrowser

VERSION = "0.1.0-beta"
MOD_NAME = "OlympianPlanetCoords"
CREDITS = {
    "author": "MightyZeus, originario de Nicaragua",
    "copyright": "Copyright (c) 2026 MightyZeus",
    "github": "https://github.com/ImMightyZeus",
    "repo": "https://github.com/ImMightyZeus/Olympian-Planet-Coords",
    "paypal": "https://paypal.me/ImMightyZeus",
    "license": "GPL-3.0",
}
# --- Textos (estandar MightyZeus 1.3, espanol / ingles) --------------------------------------
LANGS = ("es", "en")
APP_TITLE = "OLYMPIAN PLANET COORDS"
OTHER_PROJECTS = [
    ("Olympian NMS Save Editor", "https://github.com/ImMightyZeus/Olympian-NMS-Save-Editor"),
]
ALL_PROJECTS_URL = "https://github.com/ImMightyZeus"

STRINGS = {
    "es": {
        "tag_now": "AHORA", "tag_dest": "DESTINO", "orbit": "ÓRBITA",
        "waiting_mod": "Esperando al mod (¿juego abierto?)…", "off_planet": "Fuera de un planeta",
        "dest_info": "a {dist} · rumbo {bearing}° {cardinal}",
        "hint_idle": "clic para ir a coordenadas", "hint_edit": "Enter · Esc · flechas: historial",
        "hist_empty": "historial vacío", "hist_pos": "historial {i}/{n}",
        "sending": "Enviando {coords}…", "no_contact": "No se pudo contactar al mod: {err}",
        "rejected": "El mod rechazó la petición", "hotkey_fail": "No se pudo guardar la tecla",
        "hotkey_pending": "Tecla {key} guardada · se activa con Reload Mod en NMS.py",
        "hotkey_active": "Tecla {key} activa", "none": "Ninguna",
        "parse_format": "formato: usa  +12.34, -56.78  (o 12,34; -56,78)",
        "parse_lat": "latitud fuera de rango (-90..90)", "parse_lon": "longitud fuera de rango (-180..180)",
        "m_size": "Tamaño", "m_normal": "Normal", "m_large": "Grande", "m_key": "Tecla",
        "m_none_click": "Ninguna (solo clic)", "m_clear": "Quitar destino de este planeta",
        "m_about": "Acerca de…", "m_move": "Mover a la esquina superior izquierda", "m_exit": "Salir",
        "a_title": "Acerca de Olympian Planet Coords", "a_dev": "Desarrollado por MightyZeus, originario de Nicaragua",
        "a_support": "Apoya el proyecto", "a_repo": "Repositorio", "a_license": "Licencia",
        "a_thanks": "Agradecimientos:",
        "thanks": [
            "A mi querida esposa y mi amada familia, por permitirme seguir siendo un niño aun en mi adultez.",
            "Pangro Daer Barun D'Gross, por seguirme corrompiendo con videojuegos.",
            "Fenrir, porque aunque ya casi no juega, aún se presta a conversar sobre mis aventuras y desventuras.",
        ],
        "a_stance_title": "Lo que hace y lo que no hace Olympian Planet Coords",
        "a_stance_intro": ("Todo mod o editor cambia el juego, y Olympian Planet Coords no es la excepción. Lo que lo "
                           "distingue no es una superioridad moral, sino estos compromisos de seguridad:"),
        "a_stance": [
            "Solo presentará resultados que el juego podría producir por sí mismo, siguiendo sus propias reglas "
            "y límites. Nunca dejará tu partida en un estado imposible.",
            "Solo modificará aquello que se sabe con certeza; no destruirá tus saves. No aplicará funciones "
            "experimentales de alto riesgo: ya el riesgo lo corremos por ti, con pruebas reales continuamente "
            "monitoreadas. De todo lo demás podremos mostrarte una lectura, pero hasta no definir un método "
            "seguro, no ofreceremos edición.",
            "Tratará de mantener tu experiencia del juego lo más fiel posible (sin dejar de ofrecer mejoras "
            "opcionales). Verás que ofrece herramientas para resolver problemas reales, no atajos para "
            "\"tenerlo todo\".",
            "Pondrá tu seguridad primero: respaldos antes de cualquier cambio a tu partida, lo mínimo invasivo "
            "posible y avisos claros cuando algo tenga riesgo.",
            "Será transparente: código abierto, sin relación con Hello Games, y explicará qué hace y qué no hace "
            "cada función.",
        ],
        "a_ai_title": "Divulgación sobre el uso de IA",
        "a_ai": ("El uso de nuevas tecnologías forma parte del día a día de este y de otros proyectos de MightyZeus. "
                 "Olympian Planet Coords se desarrolla con la ayuda de una IA (Claude), que participa en la escritura "
                 "y revisión del código; la dirección del proyecto, qué funciones entran y cuáles se dejan fuera a "
                 "propósito, y las pruebas en el juego real son trabajo humano."),
        "a_disclaimer": "Proyecto hecho por fans, sin relación con Hello Games.",
        "a_others": "Otros proyectos de MightyZeus para No Man's Sky:", "a_all": "Todos los proyectos",
        "a_fonts": "Fuentes: DSEG7 Classic (keshikan) y Share Tech Mono (Carrois), SIL OFL 1.1",
        "a_close": "Cerrar",
    },
    "en": {
        "tag_now": "NOW", "tag_dest": "DEST", "orbit": "ORBIT",
        "waiting_mod": "Waiting for the mod (is the game running?)…", "off_planet": "Not on a planet",
        "dest_info": "{dist} away · bearing {bearing}° {cardinal}",
        "hint_idle": "click to go to coordinates", "hint_edit": "Enter · Esc · arrows: history",
        "hist_empty": "history is empty", "hist_pos": "history {i}/{n}",
        "sending": "Sending {coords}…", "no_contact": "Couldn't reach the mod: {err}",
        "rejected": "The mod rejected the request", "hotkey_fail": "Couldn't save the key",
        "hotkey_pending": "Key {key} saved · takes effect after Reload Mod in NMS.py",
        "hotkey_active": "Key {key} active", "none": "None",
        "parse_format": "format: use  +12.34, -56.78  (or 12,34; -56,78)",
        "parse_lat": "latitude out of range (-90..90)", "parse_lon": "longitude out of range (-180..180)",
        "m_size": "Size", "m_normal": "Normal", "m_large": "Large", "m_key": "Key",
        "m_none_click": "None (click only)", "m_clear": "Clear this planet's destination",
        "m_about": "About…", "m_move": "Move to the top-left corner", "m_exit": "Exit",
        "a_title": "About Olympian Planet Coords", "a_dev": "Developed by MightyZeus, from Nicaragua",
        "a_support": "Support the project", "a_repo": "Repository", "a_license": "License",
        "a_thanks": "Acknowledgements:",
        "thanks": [
            "To my dear wife and my beloved family, for letting me keep being a kid even as an adult.",
            "Pangro Daer Barun D'Gross, for continuing to corrupt me with video games.",
            "Fenrir, because even though he hardly plays anymore, he's still up for talking about my "
            "adventures and misadventures.",
        ],
        "a_stance_title": "What Olympian Planet Coords does and doesn't do",
        "a_stance_intro": ("Every mod or editor changes the game, and Olympian Planet Coords is no exception. What "
                           "sets it apart isn't moral superiority, but these safety commitments:"),
        "a_stance": [
            "It will only produce results the game could produce on its own, following its own rules and limits. "
            "It will never leave your save in an impossible state.",
            "It will only change what is known with certainty; it will not destroy your saves. It will not apply "
            "high-risk experimental features: we already take that risk for you, with real tests that are "
            "continuously monitored. For everything else we may show you a reading, but until a safe method is "
            "defined, we will not offer editing.",
            "It will try to keep your game experience as faithful as possible (while still offering optional "
            "improvements). You'll see that it offers tools that solve real problems, not shortcuts to "
            "\"have everything\".",
            "It will put your safety first: backups before any change to your save, as little intrusion as "
            "possible, and clear warnings when something carries risk.",
            "It will be transparent: open source, not affiliated with Hello Games, and it will explain what each "
            "feature does and doesn't do.",
        ],
        "a_ai_title": "AI Disclosure",
        "a_ai": ("New technologies are part of the everyday work behind this and other MightyZeus projects. "
                 "Olympian Planet Coords is developed with the help of an AI (Claude), which takes part in writing "
                 "and reviewing the code; the project's direction, which features go in and which are left out on "
                 "purpose, and the testing in the real game are human work."),
        "a_disclaimer": "Fan-made project, not affiliated with Hello Games.",
        "a_others": "Other MightyZeus projects for No Man's Sky:", "a_all": "All projects",
        "a_fonts": "Fonts: DSEG7 Classic (keshikan) and Share Tech Mono (Carrois), SIL OFL 1.1",
        "a_close": "Close",
    },
}


def system_lang() -> str:
    """OLYMPIAN_LANG si existe; si no, el idioma de Windows; si no, el locale."""
    env = os.environ.get("OLYMPIAN_LANG", "").strip().lower()[:2]
    if env in LANGS:
        return env
    try:
        if sys.platform == "win32":
            import ctypes

            lid = ctypes.windll.kernel32.GetUserDefaultUILanguage()
            return "es" if (lid & 0x3FF) == 0x0A else "en"
        import locale

        loc = (locale.getlocale()[0] or os.environ.get("LANG", "")).lower()
        return "es" if loc.startswith("es") else "en"
    except Exception:
        return "es"


class CoordsError(ValueError):
    def __init__(self, code: str):
        super().__init__(STRINGS["es"][code])
        self.code = code


# Carpeta de recursos (fuentes, icono). Empaquetado con PyInstaller, los archivos viven en sys._MEIPASS.
HERE = getattr(sys, "_MEIPASS", None) or os.path.dirname(os.path.abspath(__file__))
FONT_FILES = [os.path.join(HERE, "fonts", "DSEG7Classic-Bold.ttf"),
              os.path.join(HERE, "fonts", "ShareTechMono-Regular.ttf")]
NUM_FAMILY = "DSEG7 Classic"
TEXT_FAMILY = "Share Tech Mono"

# --- Parser (identico al del mod; una prueba verifica que coinciden) -------------------------
_NUM = r"([+-]?\d+(?:\.\d+)?)"
_COORD_DOT_RE = re.compile(rf"^{_NUM}\s*[,\s]\s*{_NUM}(?:\s*[,\s]\s*(.*))?$")
_NUM_COMMA = r"([+-]?\d+(?:[.,]\d+)?)"
_COORD_SEMI_RE = re.compile(rf"^{_NUM_COMMA}\s*;\s*{_NUM_COMMA}(?:\s*[;\s]\s*(.*))?$")


def parse_coords(text: str) -> tuple[float, float, str]:
    t = text.strip()
    m = _COORD_SEMI_RE.match(t) if ";" in t else _COORD_DOT_RE.match(t)
    if not m:
        raise CoordsError("parse_format")
    lat = float(m.group(1).replace(",", "."))
    lon = float(m.group(2).replace(",", "."))
    name = (m.group(3) or "").strip()
    if not (-90.0 <= lat <= 90.0):
        raise CoordsError("parse_lat")
    if not (-180.0 <= lon <= 180.0):
        raise CoordsError("parse_lon")
    return lat, lon, name


def seg_digits(value: float, width: int) -> tuple[str, str, str]:
    """(relleno, signo, digitos) para DSEG7. DSEG7 no tiene '+' ni ',', asi que el signo va aparte
    (en Share Tech Mono). El relleno va ANTES del signo para alinear las columnas sin separar el
    signo del numero ('!' es, en DSEG, un digito vacio del mismo ancho)."""
    digits = f"{abs(value):.2f}"
    return "!" * max(0, width - len(digits)), "-" if value < 0 else "+", digits


def format_distance(meters: float) -> str:
    return f"{meters / 1000:.1f} km" if meters >= 1000 else f"{meters:.0f} m"


# --- Flecha 3D (geometria pura; se prueba aparte) ---------------------------------------------
# Unidades: caja de 80x80 centrada en (0, 0). Se escala al tamano real del lienzo.
ARROW_VERTS = [(0.0, 0.0, 1.3), (-0.62, 0.0, -0.7), (0.62, 0.0, -0.7), (0.0, 0.0, -0.3), (0.0, 0.28, -0.4)]
ARROW_FACES = [(0, 1, 4), (0, 4, 2), (1, 3, 4), (3, 2, 4), (0, 3, 1), (0, 2, 3)]
CAMERA_TILT = -0.9  # la flecha se ve desde arriba y algo de frente
RING_R = 36.0  # radio del circulo con flecha
RING_R_IDLE = RING_R * 0.4  # en reposo: -60%
SPHERE_R = 9.0  # esfera en reposo (cabe dentro del circulo reducido sin tocarlo)
LIGHT = (0.35, 0.75, 0.55)
RING_COLOR = "#A8740F"  # dorado oscuro (mas oscuro que el ambar del titulo)
ARROW_EDGE = "#FFE7A8"
SPHERE_LAYERS = [(1.0, 0.0, "#B97A10"), (0.72, 0.18, "#E09A22"), (0.42, 0.32, "#F2B134"), (0.16, 0.42, "#FFE7A8")]


def _rot_y(p, a):
    c, s = math.cos(a), math.sin(a)
    return (p[0] * c + p[2] * s, p[1], -p[0] * s + p[2] * c)


def _rot_x(p, a):
    c, s = math.cos(a), math.sin(a)
    return (p[0], p[1] * c - p[2] * s, p[1] * s + p[2] * c)


def arrow_geometry(yaw: float, pitch: float, morph: float) -> dict:
    """Devuelve lo que hay que dibujar (en unidades de la caja 80x80, centro 0,0).

    yaw: 0 = destino de frente (flecha hacia arriba), +90 = a la derecha, 180 = detras.
    pitch: + = destino por encima (la flecha se alarga hacia arriba).
    morph: 0 = flecha completa; 1 = esfera en reposo y circulo al 40%.
    """
    morph = max(0.0, min(1.0, morph))
    ry, rp = math.radians(yaw), math.radians(pitch)
    world = [_rot_x(_rot_x(_rot_y(v, ry), -rp), CAMERA_TILT) for v in ARROW_VERTS]
    proj = []
    for q in world:
        k = 26.0 * (1.0 - morph) / (1.0 + q[2] * 0.12)
        proj.append((q[0] * k, -q[1] * k))
    faces = []
    if morph < 0.98:
        for f in ARROW_FACES:
            a, b, c = (world[i] for i in f)
            u = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
            v = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
            n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
            m = math.sqrt(n[0] ** 2 + n[1] ** 2 + n[2] ** 2) or 1.0
            lit = abs((n[0] * LIGHT[0] + n[1] * LIGHT[1] + n[2] * LIGHT[2]) / m)
            colour = "#%02x%02x%02x" % (round(170 + 80 * lit), round(105 + 80 * lit), round(15 + 50 * lit))
            faces.append({"z": (a[2] + b[2] + c[2]) / 3, "points": [proj[i] for i in f], "fill": colour})
        faces.sort(key=lambda x: -x["z"])  # pinta primero lo mas lejano
    sphere = []
    r = SPHERE_R * morph
    if r >= 0.3:
        for scale, off, colour in SPHERE_LAYERS:
            sphere.append({"cx": -r * off, "cy": -r * off, "r": r * scale, "fill": colour})
    return {"faces": faces, "ring_r": RING_R - (RING_R - RING_R_IDLE) * morph, "sphere": sphere}


def approach_angle(current: float, target: float, factor: float) -> float:
    """Acerca un angulo a otro por el camino corto (maneja el salto -180/+180)."""
    diff = (target - current + 180.0) % 360.0 - 180.0
    return current + diff * factor


SUPERSAMPLE = 4  # se dibuja a 4x y se reduce: bordes suaves


def render_arrow_image(geo: dict, size: int, ss: int = SUPERSAMPLE):
    """Dibuja la flecha/esfera/circulo con Pillow en RGBA (fondo totalmente transparente).
    Mismas proporciones que el dibujo del Canvas: caja de 80x80 escalada a 'size' pixeles."""
    from PIL import Image, ImageDraw

    big = size * ss
    img = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    k = big / 80.0
    c = big / 2.0
    px = size / 80.0  # pixeles reales por unidad
    rr = geo["ring_r"] * k
    ring_w = max(ss, round(1.8 * px * ss))
    d.ellipse((c - rr, c - rr, c + rr, c + rr), outline=RING_COLOR, width=ring_w)
    edge_w = max(1, round(0.8 * px * ss))
    for f in geo["faces"]:
        pts = [(c + x * k, c + y * k) for (x, y) in f["points"]]
        d.polygon(pts, fill=f["fill"])
        d.line(pts + [pts[0]], fill=ARROW_EDGE, width=edge_w, joint="curve")
    for sph in geo["sphere"]:
        x, y, r = c + sph["cx"] * k, c + sph["cy"] * k, sph["r"] * k
        d.ellipse((x - r, y - r, x + r, y + r), fill=sph["fill"])
    return img.resize((size, size), Image.Resampling.LANCZOS)


def premultiplied_bgra(img) -> bytes:
    """RGBA -> bytes BGRA con alfa premultiplicado (lo que pide UpdateLayeredWindow)."""
    from PIL import Image

    r, g, b, a = img.convert("RGBa").split()
    return Image.merge("RGBA", (b, g, r, a)).tobytes()


# --- Ajustes del overlay (%LOCALAPPDATA%, sin admin) ------------------------------------------
def settings_path() -> str:
    base = os.environ.get("LOCALAPPDATA") or os.path.join(os.path.expanduser("~"), ".local", "share")
    return os.path.join(base, "OlympianPlanetCoords", "overlay.json")


def load_settings(path: str) -> dict:
    data = {"size": "normal", "lang": None}
    try:
        with open(path, encoding="utf-8") as f:
            raw = json.load(f)
        if raw.get("size") in ("normal", "grande"):
            data["size"] = raw["size"]
        if raw.get("lang") in LANGS:
            data["lang"] = raw["lang"]
    except Exception:
        pass
    return data


def save_settings(path: str, data: dict) -> None:
    try:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            json.dump(data, f)
    except OSError:
        pass


# Tamanos de letra: "grande" = tamano de la v0.0.1; "normal" = la mitad.
FONT_SIZES = {"grande": {"num": 15, "text": 15, "small": 10, "entry": 14},
              "normal": {"num": 8, "text": 8, "small": 6, "entry": 8}}


# --- Cliente HTTP ----------------------------------------------------------------------------
class ModClient:
    def __init__(self, host: str, port: int, timeout: float = 0.6):
        self.base = f"http://{host}:{port}/{MOD_NAME}"
        self.timeout = timeout

    def get_coords(self) -> dict:
        with urllib.request.urlopen(f"{self.base}/coords", timeout=self.timeout) as r:
            return json.loads(r.read().decode("utf-8"))

    def _put(self, path: str, params: dict) -> dict:
        url = f"{self.base}/{path}?{urllib.parse.urlencode(params)}"
        req = urllib.request.Request(url, method="PUT", data=b"")
        with urllib.request.urlopen(req, timeout=2.0) as r:
            return json.loads(r.read().decode("utf-8"))

    def place_marker(self, lat: float, lon: float, name: str) -> dict:
        return self._put("marker", {"lat": f"{lat}", "lon": f"{lon}", "name": name})

    def clear_marker(self) -> dict:
        return self._put("clear", {"confirm": "true"})

    def set_lang(self, code: str) -> dict:
        return self._put("lang", {"code": code})

    def set_hotkey(self, key: str) -> dict:
        return self._put("hotkey", {"key": key})


# --- Fuentes ---------------------------------------------------------------------------------
def load_private_fonts() -> None:
    """Windows: AddFontResourceExW con FR_PRIVATE (solo este proceso, sin admin)."""
    if sys.platform != "win32":
        return
    try:
        import ctypes

        FR_PRIVATE = 0x10
        for p in FONT_FILES:
            if os.path.exists(p):
                ctypes.windll.gdi32.AddFontResourceExW(p, FR_PRIVATE, 0)
    except Exception:
        pass


def _enable_dpi_awareness():
    if sys.platform == "win32":
        try:
            import ctypes

            ctypes.windll.shcore.SetProcessDpiAwareness(1)
        except Exception:
            pass


# --- Ventana con alfa por pixel (Windows) -----------------------------------------------------
class LayeredArrowWindow:
    """Toplevel sin bordes que Windows compone con alfa real por pixel (UpdateLayeredWindow).
    Los pixeles totalmente transparentes dejan pasar el clic al juego."""

    GWL_EXSTYLE = -20
    WS_EX_LAYERED = 0x00080000
    WS_EX_TOOLWINDOW = 0x00000080  # sin boton en la barra de tareas
    ULW_ALPHA = 0x00000002
    AC_SRC_OVER = 0x00
    AC_SRC_ALPHA = 0x01

    def __init__(self, root: tk.Tk, constant_alpha: int = 235):
        import ctypes
        from ctypes import wintypes as wt

        self._ct = ctypes
        # Instancias propias de las DLL: los argtypes no afectan a nadie mas en el proceso.
        u = ctypes.WinDLL("user32", use_last_error=True)
        g = ctypes.WinDLL("gdi32", use_last_error=True)
        LONG_PTR = ctypes.c_ssize_t

        class BLENDFUNCTION(ctypes.Structure):
            _fields_ = [("BlendOp", ctypes.c_ubyte), ("BlendFlags", ctypes.c_ubyte),
                        ("SourceConstantAlpha", ctypes.c_ubyte), ("AlphaFormat", ctypes.c_ubyte)]

        class BITMAPINFOHEADER(ctypes.Structure):
            _fields_ = [("biSize", wt.DWORD), ("biWidth", wt.LONG), ("biHeight", wt.LONG),
                        ("biPlanes", wt.WORD), ("biBitCount", wt.WORD), ("biCompression", wt.DWORD),
                        ("biSizeImage", wt.DWORD), ("biXPelsPerMeter", wt.LONG),
                        ("biYPelsPerMeter", wt.LONG), ("biClrUsed", wt.DWORD), ("biClrImportant", wt.DWORD)]

        class BITMAPINFO(ctypes.Structure):
            _fields_ = [("bmiHeader", BITMAPINFOHEADER), ("bmiColors", wt.DWORD * 3)]

        u.GetDC.argtypes, u.GetDC.restype = [wt.HWND], wt.HDC
        u.ReleaseDC.argtypes, u.ReleaseDC.restype = [wt.HWND, wt.HDC], ctypes.c_int
        u.GetWindowLongPtrW.argtypes, u.GetWindowLongPtrW.restype = [wt.HWND, ctypes.c_int], LONG_PTR
        u.SetWindowLongPtrW.argtypes, u.SetWindowLongPtrW.restype = [wt.HWND, ctypes.c_int, LONG_PTR], LONG_PTR
        u.UpdateLayeredWindow.argtypes = [wt.HWND, wt.HDC, ctypes.POINTER(wt.POINT), ctypes.POINTER(wt.SIZE),
                                          wt.HDC, ctypes.POINTER(wt.POINT), wt.COLORREF,
                                          ctypes.POINTER(BLENDFUNCTION), wt.DWORD]
        u.UpdateLayeredWindow.restype = wt.BOOL
        g.CreateCompatibleDC.argtypes, g.CreateCompatibleDC.restype = [wt.HDC], wt.HDC
        g.DeleteDC.argtypes, g.DeleteDC.restype = [wt.HDC], wt.BOOL
        g.CreateDIBSection.argtypes = [wt.HDC, ctypes.POINTER(BITMAPINFO), wt.UINT,
                                       ctypes.POINTER(ctypes.c_void_p), wt.HANDLE, wt.DWORD]
        g.CreateDIBSection.restype = wt.HBITMAP
        g.SelectObject.argtypes, g.SelectObject.restype = [wt.HDC, wt.HGDIOBJ], wt.HGDIOBJ
        g.DeleteObject.argtypes, g.DeleteObject.restype = [wt.HGDIOBJ], wt.BOOL
        self._u, self._g, self._wt = u, g, wt
        self._BITMAPINFO, self._BITMAPINFOHEADER = BITMAPINFO, BITMAPINFOHEADER
        self._blend = BLENDFUNCTION(self.AC_SRC_OVER, 0, max(0, min(255, constant_alpha)), self.AC_SRC_ALPHA)

        self.win = tk.Toplevel(root)
        self.win.overrideredirect(True)
        self.win.attributes("-topmost", True)
        self.win.geometry("1x1+-200+-200")
        self.win.update_idletasks()
        self.hwnd = int(self.win.wm_frame(), 16)  # ventana "envoltura" real de Tk
        style = u.GetWindowLongPtrW(self.hwnd, self.GWL_EXSTYLE)
        u.SetWindowLongPtrW(self.hwnd, self.GWL_EXSTYLE, style | self.WS_EX_LAYERED | self.WS_EX_TOOLWINDOW)

    def show(self, bgra: bytes, width: int, height: int, x: int, y: int) -> None:
        ct, u, g, wt = self._ct, self._u, self._g, self._wt
        if len(bgra) != width * height * 4:
            raise ValueError("tamano de imagen inconsistente")
        screen = u.GetDC(None)
        if not screen:
            raise OSError(ct.get_last_error(), "GetDC")
        mem = hbmp = old = None
        try:
            mem = g.CreateCompatibleDC(screen)
            if not mem:
                raise OSError(ct.get_last_error(), "CreateCompatibleDC")
            bmi = self._BITMAPINFO()
            bmi.bmiHeader.biSize = ct.sizeof(self._BITMAPINFOHEADER)
            bmi.bmiHeader.biWidth = width
            bmi.bmiHeader.biHeight = -height  # de arriba hacia abajo
            bmi.bmiHeader.biPlanes = 1
            bmi.bmiHeader.biBitCount = 32
            bmi.bmiHeader.biCompression = 0  # BI_RGB
            bits = ct.c_void_p()
            hbmp = g.CreateDIBSection(screen, ct.byref(bmi), 0, ct.byref(bits), None, 0)
            if not hbmp or not bits.value:
                raise OSError(ct.get_last_error(), "CreateDIBSection")
            ct.memmove(bits, bgra, len(bgra))
            old = g.SelectObject(mem, hbmp)
            ok = u.UpdateLayeredWindow(self.hwnd, screen, ct.byref(wt.POINT(x, y)), ct.byref(wt.SIZE(width, height)),
                                       mem, ct.byref(wt.POINT(0, 0)), 0, ct.byref(self._blend), self.ULW_ALPHA)
            if not ok:
                raise OSError(ct.get_last_error(), "UpdateLayeredWindow")
        finally:
            if mem and old:
                g.SelectObject(mem, old)
            if hbmp:
                g.DeleteObject(hbmp)
            if mem:
                g.DeleteDC(mem)
            u.ReleaseDC(None, screen)

    def bind(self, seq, fn):
        self.win.bind(seq, fn)

    def destroy(self):
        try:
            self.win.destroy()
        except tk.TclError:
            pass


def pillow_available() -> bool:
    try:
        import PIL.Image  # noqa: F401
        import PIL.ImageDraw  # noqa: F401

        return True
    except Exception:
        return False


# --- Ventana ---------------------------------------------------------------------------------
BG = "#101418"
KEY = "#010203"  # color que Windows vuelve transparente (fondo de la flecha)
FG = "#E8F1F2"
ACCENT = "#F2B134"
MUTED = "#8899A6"
ERR = "#FF6B6B"
OK = "#7BE495"
DEST = "#7FD4FF"
HOTKEY_CHOICES = ("f5", "f6", "f7", "f8", "f9", "f10", "f11", "ninguna")  # igual que en el mod
FRAME_MS = 33  # ~30 fps para la flecha
POLL_MS = 60  # consulta al mod ~16 veces por segundo
MORPH_SECONDS = 0.5


ICON_ICO = os.path.join(HERE, "olympian_planet_coords.ico")
ICON_PNG = os.path.join(HERE, "olympian_planet_coords.png")


def load_logo_photo(master, size: int):
    """Logo del mod como PhotoImage de Tk (con Pillow si esta; si no, el PNG reducido con subsample)."""
    try:
        from PIL import Image, ImageTk

        img = Image.open(ICON_PNG).convert("RGBA").resize((size, size), Image.Resampling.LANCZOS)
        return ImageTk.PhotoImage(img, master=master)
    except Exception:
        pass
    try:
        photo = tk.PhotoImage(master=master, file=ICON_PNG)
        factor = max(1, photo.width() // size)
        return photo.subsample(factor, factor)
    except Exception:
        return None


def set_window_icon(win, owner) -> None:
    """Icono del mod en la ventana (barra de titulo / Alt+Tab). Nunca falla."""
    try:
        if sys.platform == "win32" and os.path.exists(ICON_ICO):
            win.iconbitmap(default=ICON_ICO)
            return
        if owner._icon_photo is None:
            owner._icon_photo = load_logo_photo(win, 64)
        if owner._icon_photo is not None:
            win.iconphoto(False, owner._icon_photo)
    except Exception:
        pass


class CoordRow(tk.Frame):
    """Una fila 'ETIQUETA  ±dd.dd, ±ddd.dd' con numeros en DSEG7 y signos en Share Tech Mono."""

    def __init__(self, master, label: str, colour: str, fonts: dict):
        super().__init__(master, bg=BG)
        self.colour = colour
        self.tag = tk.Label(self, text=label, fg=MUTED, bg=BG, font=fonts["small"], width=8, anchor="w")
        self.tag.pack(side="left")
        self.parts = []
        for kind in ("pad", "sign", "num", "sep", "pad", "sign", "num"):
            lbl = tk.Label(self, bg=BG, fg=colour, padx=0, bd=0,
                           font=fonts["num"] if kind in ("num", "pad") else fonts["text"])
            lbl.pack(side="left")
            self.parts.append(lbl)
        self.parts[3].configure(text=", ")
        self.extra = tk.Label(self, bg=BG, fg=colour, font=fonts["text"])
        self.extra.pack(side="left", padx=(6, 0))

    def set(self, lat: float, lon: float, extra: str = ""):
        texts = (*seg_digits(lat, 5), None, *seg_digits(lon, 6))
        for lbl, txt in zip(self.parts, texts, strict=True):
            if txt is not None:
                lbl.configure(text=txt, fg=self.colour)
        self.extra.configure(text=extra)

    def text(self) -> str:
        return "".join(p.cget("text") for p in self.parts).replace("!", " ")

    def bind_all_children(self, seq, fn):
        for w in [self, self.tag, self.extra, *self.parts]:
            w.bind(seq, fn)


class Overlay:
    def __init__(self, client: ModClient, x: int, y: int, poll_ms: int = POLL_MS,
                 settings_file: str | None = None):
        self.client = client
        self.poll_ms = poll_ms
        self.settings_file = settings_file or settings_path()
        self.settings = load_settings(self.settings_file)
        self.lang = self.settings.get("lang") or system_lang()
        self._lang_push_busy = False
        self.editing = False
        self.last_data: dict | None = None
        self.last_input_request_id: int | None = None
        self.last_seq: int | None = None
        self.history: list = []
        self.hist_pos = -1
        self.status_until = 0.0
        self.status_sticky = False
        self._results: "queue.Queue[tuple[str, object]]" = queue.Queue()
        self._poll_busy = False
        self._next_poll = 0.0
        # estado animado de la flecha
        self.arrow_yaw = 0.0
        self.arrow_pitch = 0.0
        self.morph = 1.0  # empieza en reposo (esfera)
        self._last_frame = time.monotonic()
        self.arrow_win: LayeredArrowWindow | None = None
        self._arrow_key = None
        self.arrow_mode = "canvas"

        load_private_fonts()
        self.root = tk.Tk()
        self.root.title("Olympian Planet Coords")
        self.root.overrideredirect(True)
        self.root.attributes("-topmost", True)
        try:
            self.root.attributes("-alpha", 0.92)
        except tk.TclError:
            pass
        self.transparent_ok = True
        try:
            self.root.attributes("-transparentcolor", KEY)  # solo Windows
        except tk.TclError:
            self.transparent_ok = False
        self.root.configure(bg=KEY)
        self.root.geometry(f"+{x}+{y}")
        self._icon_photo = None
        set_window_icon(self.root, self)

        fams = set(tkfont.families(self.root))
        num_fam = NUM_FAMILY if NUM_FAMILY in fams else "Consolas"
        txt_fam = TEXT_FAMILY if TEXT_FAMILY in fams else "Consolas"
        self.fonts_ok = NUM_FAMILY in fams and TEXT_FAMILY in fams
        sizes = FONT_SIZES[self.settings["size"]]
        self.fonts = {
            "num": tkfont.Font(self.root, family=num_fam, size=sizes["num"], weight="bold"),
            "text": tkfont.Font(self.root, family=txt_fam, size=sizes["text"]),
            "small": tkfont.Font(self.root, family=txt_fam, size=sizes["small"]),
            "entry": tkfont.Font(self.root, family=txt_fam, size=sizes["entry"]),
        }

        self.outer = tk.Frame(self.root, bg=KEY)
        self.outer.pack()
        self.frame = tk.Frame(self.outer, bg=BG, padx=10, pady=6, highlightthickness=1,
                              highlightbackground="#2C3A44")
        self.frame.pack(side="left", anchor="n")
        self.canvas = tk.Canvas(self.outer, bg=KEY, highlightthickness=0, bd=0, width=60, height=60)
        if sys.platform == "win32" and pillow_available():
            try:
                self.arrow_win = LayeredArrowWindow(self.root)
                self.arrow_win.bind("<Button-1>", self.start_edit)
                self.arrow_win.bind("<Button-3>", self.show_menu)
                self.arrow_mode = "layered"
            except Exception as e:  # sin la ventana con alfa se usa el Canvas de siempre
                print(f"Flecha suave no disponible ({e}); se usa el dibujo simple.", file=sys.stderr)
                self.arrow_win = None
        if self.arrow_win is None:
            self.canvas.pack(side="left", anchor="n", padx=(6, 0))

        head = tk.Frame(self.frame, bg=BG)
        head.pack(fill="x")
        self.head = head
        self.title = tk.Label(head, text=APP_TITLE, fg=ACCENT, bg=BG, font=self.fonts["small"], anchor="w")
        self.title.pack(side="left")
        self.orbit_tag = tk.Label(head, text="", fg=DEST, bg=BG, font=self.fonts["small"])
        self.orbit_tag.pack(side="left", padx=(8, 0))

        self.message = tk.Label(self.frame, text=self.tr("waiting_mod"), fg=MUTED, bg=BG,
                                font=self.fonts["text"], anchor="w")
        self.now_row = CoordRow(self.frame, self.tr("tag_now"), FG, self.fonts)
        self.dest_row = CoordRow(self.frame, self.tr("tag_dest"), DEST, self.fonts)
        self.dest_info = tk.Label(self.frame, text="", fg=DEST, bg=BG, font=self.fonts["small"], anchor="w")
        self.entry = tk.Entry(self.frame, font=self.fonts["entry"], bg="#1C242B", fg=FG,
                              insertbackground=FG, relief="flat", width=26)
        self.status = tk.Label(self.frame, text=self.hint_idle(), fg=MUTED, bg=BG, font=self.fonts["small"], anchor="w")
        self._layout = None
        self._show(["message"])

        for w in (self.frame, head, self.title, self.orbit_tag, self.message, self.dest_info, self.status,
                  self.canvas):
            w.bind("<Button-1>", self.start_edit)
            w.bind("<Button-3>", self.show_menu)
        for row in (self.now_row, self.dest_row):
            row.bind_all_children("<Button-1>", self.start_edit)
            row.bind_all_children("<Button-3>", self.show_menu)
        self.entry.bind("<Return>", self.submit)
        self.entry.bind("<KP_Enter>", self.submit)
        self.entry.bind("<Escape>", self.cancel_edit)
        self.entry.bind("<Up>", lambda e: self.history_step(+1))
        self.entry.bind("<Down>", lambda e: self.history_step(-1))
        self.entry.bind("<Button-3>", self.show_menu)

        self.size_var = tk.StringVar(self.root, value=self.settings["size"])
        self.hotkey_var = tk.StringVar(self.root, value="")
        self.lang_var = tk.StringVar(self.root, value=self.lang)
        self.menu = None
        self._build_menu()

        self.root.after(30, self._tick)
        self.root.after(FRAME_MS, self._render_loop)

    # ---- idioma
    def tr(self, _key: str, /, **kw):
        val = STRINGS.get(self.lang, STRINGS["es"])[_key]
        return val.format(**kw) if (kw and isinstance(val, str)) else val

    def _build_menu(self):
        if self.menu is not None:
            self.menu.destroy()
        self.menu = tk.Menu(self.root, tearoff=0)
        size_menu = tk.Menu(self.menu, tearoff=0)
        size_menu.add_radiobutton(label=self.tr("m_normal"), value="normal", variable=self.size_var,
                                  command=lambda: self.set_size("normal"))
        size_menu.add_radiobutton(label=self.tr("m_large"), value="grande", variable=self.size_var,
                                  command=lambda: self.set_size("grande"))
        self.menu.add_cascade(label=self.tr("m_size"), menu=size_menu)
        key_menu = tk.Menu(self.menu, tearoff=0)
        for key in HOTKEY_CHOICES:
            key_menu.add_radiobutton(label=key.upper() if key != "ninguna" else self.tr("m_none_click"),
                                     value=key, variable=self.hotkey_var, command=lambda k=key: self.set_hotkey(k))
        self.menu.add_cascade(label=self.tr("m_key"), menu=key_menu)
        lang_menu = tk.Menu(self.menu, tearoff=0)
        lang_menu.add_radiobutton(label="Español", value="es", variable=self.lang_var,
                                  command=lambda: self.set_lang("es"))
        lang_menu.add_radiobutton(label="English", value="en", variable=self.lang_var,
                                  command=lambda: self.set_lang("en"))
        self.menu.add_cascade(label="Idioma / Language", menu=lang_menu)
        self.menu.add_command(label=self.tr("m_clear"), command=self.clear_marker)
        self.menu.add_command(label=self.tr("m_about"), command=self.show_about)
        self.menu.add_command(label=self.tr("m_move"), command=lambda: self.move_to(12, 12))
        self.menu.add_separator()
        self.menu.add_command(label=self.tr("m_exit"), command=self.root.destroy)

    def set_lang(self, code: str):
        if code not in LANGS:
            return
        self.lang = code
        self.lang_var.set(code)
        self.settings["lang"] = code
        save_settings(self.settings_file, self.settings)
        self.now_row.tag.configure(text=self.tr("tag_now"))
        self.dest_row.tag.configure(text=self.tr("tag_dest"))
        self._build_menu()
        if self.last_data is None:
            self.message.configure(text=self.tr("waiting_mod"))
        if not self.editing and not self.status_sticky:
            self.set_status(self.hint_idle(), MUTED, 0)
        self._push_lang()

    def _push_lang(self):
        """El idioma del overlay manda: se envia al mod para que sus mensajes coincidan."""
        if self._lang_push_busy:
            return
        self._lang_push_busy = True

        def work():
            try:
                self.client.set_lang(self.lang)
            except Exception:
                pass
            finally:
                self._lang_push_busy = False

        threading.Thread(target=work, daemon=True).start()

    # ---- tamano
    def set_size(self, size: str):
        if size not in FONT_SIZES:
            return
        self.settings["size"] = size
        self.size_var.set(size)
        for key, font in self.fonts.items():
            font.configure(size=FONT_SIZES[size][key])
        save_settings(self.settings_file, self.settings)
        self._arrow_key = None
        self.draw_arrow()

    # ---- tecla
    def hint_idle(self) -> str:
        key = (self.last_data or {}).get("hotkey") if getattr(self, "last_data", None) else None
        if key and key != "ninguna":
            return f"{self.tr('hint_idle')} · {key.upper()}"
        return self.tr("hint_idle")

    def set_hotkey(self, key: str):
        def work():
            try:
                self._results.put(("hotkey", self.client.set_hotkey(key)))
            except Exception as e:
                self._results.put(("error", self.tr("no_contact", err=e)))

        threading.Thread(target=work, daemon=True).start()

    # ---- distribucion
    def _show(self, names: list):
        if names == self._layout:
            return
        for w in (self.message, self.now_row, self.dest_row, self.dest_info, self.entry):
            w.pack_forget()
        self.status.pack_forget()
        widgets = {"message": self.message, "now": self.now_row, "dest": self.dest_row,
                   "dest_info": self.dest_info, "entry": self.entry}
        for n in names:
            widgets[n].pack(fill="x", anchor="w")
        self.status.pack(fill="x")
        self._layout = names

    def _view_rows(self) -> list:
        d = self.last_data or {}
        if d.get("lat") is None:
            return ["message"]
        rows = ["now"]
        if d.get("destination"):
            rows += ["dest", "dest_info"]
        return rows

    # ---- flecha
    def _arrow_target(self):
        """(yaw, pitch, morph_objetivo) segun los ultimos datos del mod."""
        d = (self.last_data or {}).get("destination") or {}
        if d and d.get("yaw") is not None:
            return float(d["yaw"]), float(d.get("pitch") or 0.0), 0.0
        return self.arrow_yaw, self.arrow_pitch, 1.0

    def _render_loop(self):
        try:
            now = time.monotonic()
            dt = min(0.2, now - self._last_frame)
            self._last_frame = now
            yaw, pitch, morph_target = self._arrow_target()
            smooth = 1.0 - math.exp(-dt * 12.0)
            self.arrow_yaw = approach_angle(self.arrow_yaw, yaw, smooth)
            self.arrow_pitch += (pitch - self.arrow_pitch) * smooth
            step = dt / MORPH_SECONDS
            if self.morph < morph_target:
                self.morph = min(morph_target, self.morph + step)
            elif self.morph > morph_target:
                self.morph = max(morph_target, self.morph - step)
            self.draw_arrow()
        finally:
            self.root.after(FRAME_MS, self._render_loop)

    def draw_arrow(self):
        if self.arrow_win is not None:
            try:
                self._draw_arrow_layered()
                return
            except Exception as e:  # si Windows rechaza la ventana con alfa, volver al Canvas
                print(f"Flecha suave desactivada ({e}); se usa el dibujo simple.", file=sys.stderr)
                self.arrow_win.destroy()
                self.arrow_win = None
                self.arrow_mode = "canvas"
                self.canvas.pack(side="left", anchor="n", padx=(6, 0))
        self._draw_arrow_canvas()

    def _draw_arrow_layered(self):
        size = max(24, self.frame.winfo_height() or 60)
        x = self.frame.winfo_rootx() + self.frame.winfo_width() + 6
        y = self.frame.winfo_rooty()
        key = (round(self.arrow_yaw, 1), round(self.arrow_pitch, 1), round(self.morph, 3), size, x, y)
        if key == self._arrow_key:
            return  # nada cambio: no redibujar
        geo = arrow_geometry(self.arrow_yaw, self.arrow_pitch, self.morph)
        img = render_arrow_image(geo, size)
        self.arrow_win.show(premultiplied_bgra(img), size, size, x, y)
        self._arrow_key = key

    def _draw_arrow_canvas(self):
        size = max(24, self.frame.winfo_height() or 60)
        if int(self.canvas.cget("width")) != size:
            self.canvas.configure(width=size, height=size)
        geo = arrow_geometry(self.arrow_yaw, self.arrow_pitch, self.morph)
        k = size / 80.0
        c = size / 2.0
        cv = self.canvas
        cv.delete("all")
        rr = geo["ring_r"] * k
        cv.create_oval(c - rr, c - rr, c + rr, c + rr, outline=RING_COLOR, width=max(1, round(1.8 * k)))
        for f in geo["faces"]:
            pts = [coord for (px, py) in f["points"] for coord in (c + px * k, c + py * k)]
            cv.create_polygon(pts, fill=f["fill"], outline=ARROW_EDGE, width=max(1, round(0.8 * k)))
        for s in geo["sphere"]:
            x, y, r = c + s["cx"] * k, c + s["cy"] * k, s["r"] * k
            cv.create_oval(x - r, y - r, x + r, y + r, fill=s["fill"], outline="")

    # ---- edicion
    def start_edit(self, _event=None):
        if self.editing:
            return
        self.editing = True
        self.hist_pos = -1
        self.entry.delete(0, tk.END)
        rows = self._view_rows()
        self._show(["now", "entry"] if "now" in rows else ["entry"])
        self.set_status(self.tr("hint_edit"), MUTED, 30)
        self.root.lift()
        self.root.focus_force()
        self.entry.focus_set()

    def cancel_edit(self, _event=None):
        if not self.editing:
            return
        self.editing = False
        self._show(self._view_rows())
        if not self.status_sticky:
            self.set_status(self.hint_idle(), MUTED, 0)

    def history_step(self, delta: int):
        if not self.history:
            self.set_status(self.tr("hist_empty"), MUTED, 3)
            return "break"
        self.hist_pos = max(-1, min(len(self.history) - 1, self.hist_pos + delta))
        self.entry.delete(0, tk.END)
        if self.hist_pos >= 0:
            h = self.history[self.hist_pos]
            txt = f"{h['lat']:+.2f}, {h['lon']:+.2f}"
            if h.get("name"):
                txt += f" {h['name']}"
            self.entry.insert(0, txt)
            self.set_status(self.tr("hist_pos", i=self.hist_pos + 1, n=len(self.history)), MUTED, 30)
        return "break"

    def submit(self, _event=None):
        try:
            lat, lon, name = parse_coords(self.entry.get())
        except ValueError as e:
            self.set_status(self.tr(e.code) if isinstance(e, CoordsError) else str(e), ERR, 5)
            return "break"
        self.status_sticky = False
        self.cancel_edit()
        self.set_status(self.tr("sending", coords=f"{lat:+.2f}, {lon:+.2f}"), ACCENT, 5)
        threading.Thread(target=self._do_place, args=(lat, lon, name), daemon=True).start()
        return "break"

    def _do_place(self, lat, lon, name):
        try:
            self._results.put(("placed", self.client.place_marker(lat, lon, name)))
        except Exception as e:
            self._results.put(("error", self.tr("no_contact", err=e)))

    def clear_marker(self):
        def work():
            try:
                self._results.put(("cleared", self.client.clear_marker()))
            except Exception as e:
                self._results.put(("error", self.tr("no_contact", err=e)))

        threading.Thread(target=work, daemon=True).start()

    def show_menu(self, event):
        self.menu.tk_popup(event.x_root, event.y_root)

    def move_to(self, x, y):
        self.root.geometry(f"+{x}+{y}")

    def show_about(self):
        win = tk.Toplevel(self.root)
        win.title(self.tr("a_title"))
        win.configure(bg=BG, padx=16, pady=12)
        win.attributes("-topmost", True)
        set_window_icon(win, self)
        f = (self.fonts["text"].cget("family"), 12)
        s = (self.fonts["small"].cget("family"), 9)
        b = (self.fonts["small"].cget("family"), 10)
        wrap = 430

        def text(parent, t, fg=MUTED, font=s, pady=0):
            tk.Label(parent, text=t, fg=fg, bg=BG, font=font, wraplength=wrap, justify="left").pack(
                anchor="w", pady=(pady, 0))

        def link(parent, t, url):
            lbl = tk.Label(parent, text=t, fg=DEST, bg=BG, font=s, cursor="hand2")
            lbl.pack(anchor="w")
            lbl.bind("<Button-1>", lambda e, u=url: webbrowser.open(u))

        # Encabezado: logo + nombre, version, autor y licencia
        top = tk.Frame(win, bg=BG)
        top.pack(anchor="w", fill="x")
        logo = load_logo_photo(win, 64)
        if logo is not None:
            win._logo = logo  # evita que el recolector borre la imagen
            tk.Label(top, image=logo, bg=BG).pack(side="left", padx=(0, 10))
        head = tk.Frame(top, bg=BG)
        head.pack(side="left", anchor="w")
        tk.Label(head, text=f"Olympian Planet Coords v{VERSION}", fg=ACCENT, bg=BG, font=f).pack(anchor="w")
        tk.Label(head, text=self.tr("a_dev"), fg=FG, bg=BG, font=s).pack(anchor="w")
        tk.Label(head, text=f"Copyright © 2026 MightyZeus · {self.tr('a_license')} {CREDITS['license']}",
                 fg=FG, bg=BG, font=s).pack(anchor="w")

        # Dos columnas para que la ventana quepa en pantallas de 1080 px con escalado
        cols = tk.Frame(win, bg=BG)
        cols.pack(anchor="w", fill="x", pady=(8, 0))
        left = tk.Frame(cols, bg=BG)
        left.pack(side="left", anchor="n", padx=(0, 18))
        right = tk.Frame(cols, bg=BG)
        right.pack(side="left", anchor="n")

        link(left, f"{self.tr('a_support')}: {CREDITS['paypal']}", CREDITS["paypal"])
        link(left, f"{self.tr('a_repo')}: {CREDITS['repo']}", CREDITS["repo"])
        text(left, self.tr("a_thanks"), FG, b, 8)
        for line in self.tr("thanks"):
            text(left, line)
        text(left, self.tr("a_ai_title"), FG, b, 8)
        text(left, self.tr("a_ai"))
        text(left, self.tr("a_disclaimer"), MUTED, s, 6)
        text(left, self.tr("a_others"), FG, b, 8)
        for label, url in OTHER_PROJECTS:
            link(left, f"• {label}", url)
        link(left, f"• {self.tr('a_all')}", ALL_PROJECTS_URL)
        text(left, self.tr("a_fonts"), MUTED, s, 8)

        text(right, self.tr("a_stance_title"), FG, b)
        text(right, self.tr("a_stance_intro"))
        for n, line in enumerate(self.tr("a_stance"), 1):
            text(right, f"{n}. {line}", pady=3)

        tk.Button(win, text=self.tr("a_close"), command=win.destroy).pack(anchor="e", pady=(10, 0))

    def set_status(self, text, colour, seconds, sticky=False):
        """seconds=0 y sticky=False: texto permanente normal. sticky=True: queda hasta la siguiente accion."""
        self.status.configure(text=text, fg=colour)
        self.status_sticky = sticky
        self.status_until = time.time() + seconds if (seconds and not sticky) else 0.0

    # ---- sondeo
    def _poll(self):
        try:
            self._results.put(("coords", self.client.get_coords()))
        except Exception as e:
            self._results.put(("offline", str(e)))
        finally:
            self._poll_busy = False

    def _tick(self):
        now = time.monotonic()
        if not self._poll_busy and now >= self._next_poll:
            self._poll_busy = True
            self._next_poll = now + self.poll_ms / 1000.0
            threading.Thread(target=self._poll, daemon=True).start()
        self._drain()
        if self.status_until and not self.status_sticky and time.time() > self.status_until and not self.editing:
            self.set_status(self.hint_idle(), MUTED, 0)
        self.root.after(30, self._tick)

    def _drain(self):
        while True:
            try:
                kind, payload = self._results.get_nowait()
            except queue.Empty:
                return
            if kind == "coords":
                self._on_coords(payload)  # type: ignore[arg-type]
            elif kind == "offline":
                self.last_data = None
                self.message.configure(text=self.tr("waiting_mod"), fg=MUTED)
                self.orbit_tag.configure(text="")
                if not self.editing:
                    self._show(["message"])
            elif kind == "placed":
                res = payload if isinstance(payload, dict) else {}
                if not res.get("queued"):
                    self.set_status(res.get("message", self.tr("rejected")), ERR, 0, sticky=True)
            elif kind == "hotkey":
                res = payload if isinstance(payload, dict) else {}
                if not res.get("saved"):
                    self.set_status(res.get("message", self.tr("hotkey_fail")), ERR, 0, sticky=True)
                else:
                    shown = res["hotkey_saved"].upper() if res["hotkey_saved"] != "ninguna" else self.tr("none")
                    if res.get("pending_reload"):
                        self.set_status(self.tr("hotkey_pending", key=shown), ACCENT, 10)
                    else:
                        self.set_status(self.tr("hotkey_active", key=shown), OK, 5)
            elif kind == "error":
                self.set_status(str(payload), ERR, 0, sticky=True)

    def _on_coords(self, data: dict):
        self.last_data = data
        self.history = data.get("history") or []
        if data.get("lang") in LANGS and data["lang"] != self.lang:
            self._push_lang()  # el mod habla otro idioma: se le envia el del overlay
        if data.get("hotkey_saved") in HOTKEY_CHOICES and self.hotkey_var.get() != data["hotkey_saved"]:
            self.hotkey_var.set(data["hotkey_saved"])
        if not self.editing and not self.status_sticky and not self.status_until:
            hint = self.hint_idle()
            if self.status.cget("text") != hint:
                self.status.configure(text=hint, fg=MUTED)
        if data.get("lat") is not None:
            self.now_row.set(data["lat"], data["lon"])
            self.orbit_tag.configure(text=self.tr("orbit") if data.get("in_orbit") else "")
        else:
            self.message.configure(text=data.get("reason") or self.tr("off_planet"), fg=MUTED)
            self.orbit_tag.configure(text="")
        dest = data.get("destination")
        if dest:
            self.dest_row.set(dest["lat"], dest["lon"], dest.get("name", ""))
            self.dest_info.configure(
                text=self.tr("dest_info", dist=format_distance(dest["distance_m"]), bearing=dest["bearing"],
                             cardinal=dest["cardinal"]))
        if not self.editing:
            self._show(self._view_rows())
        # Resultado que reporta el juego (destino puesto, llegada, error real)
        res = data.get("last_result") or {}
        seq = res.get("seq")
        if self.last_seq is None:
            self.last_seq = seq
        elif seq != self.last_seq:
            self.last_seq = seq
            if res.get("message"):
                if res.get("ok"):
                    self.set_status(res["message"], OK, 6)
                else:
                    self.set_status(res["message"], ERR, 0, sticky=True)  # queda hasta la siguiente accion
        # Tecla dentro del juego (F9 por defecto)
        req = data.get("input_request_id")
        if self.last_input_request_id is None:
            self.last_input_request_id = req
        elif req != self.last_input_request_id:
            self.last_input_request_id = req
            self.start_edit()

    def run(self):
        self.root.mainloop()


def main(argv=None):
    ap = argparse.ArgumentParser(description="Overlay de coordenadas para No Man's Sky (Olympian)")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=5000)
    ap.add_argument("--x", type=int, default=12)
    ap.add_argument("--y", type=int, default=12)
    ap.add_argument("--poll-ms", type=int, default=POLL_MS)
    args = ap.parse_args(argv)
    _enable_dpi_awareness()
    Overlay(ModClient(args.host, args.port), args.x, args.y, args.poll_ms).run()


if __name__ == "__main__":
    main()
