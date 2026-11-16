# /// script
# dependencies = ["nmspy>=185034.0"]
# ///
"""
Olympian Planet Coords v0.0.3  -  mod de NMS.py para No Man's Sky (PC / Windows)

Olympian Planet Coords — Copyright © 2026 MightyZeus · Licencia GPL-3.0
Apoya el proyecto: https://paypal.me/ImMightyZeus · Repositorio: https://github.com/ImMightyZeus/Olympian-Planet-Coords

Agradecimientos:
A mi querida esposa y mi amada familia, por permitirme seguir siendo un niño aun en mi adultez.
Pangro Daer Barun D'Gross, por seguirme corrompiendo con videojuegos.
Fenrir, porque aunque ya casi no juega, aún se presta a conversar sobre mis aventuras y desventuras.

Este programa es software libre: puedes redistribuirlo y/o modificarlo bajo los terminos de la
GNU General Public License version 3 (archivo LICENSE). Proyecto hecho por fans, sin relacion con
Hello Games. Se desarrolla con la ayuda de una IA (Claude); la direccion del proyecto y las pruebas
en el juego real son trabajo humano.

Que hace
--------
* Calcula en tiempo real la latitud / longitud del jugador sobre el planeta (o del punto del
  planeta que tiene debajo cuando esta en orbita).
* Un destino por planeta: se escribe "lat, lon [nombre]" y se coloca un marcador. El destino
  se recuerda por planeta (aunque te vayas) y solo se muestra cuando estas en ese planeta.
* Tecla para abrir el campo de texto: F9 por defecto (F8 la usa Imperial Galactic Map);
  se cambia desde el overlay (clic derecho -> Tecla) y se aplica con "Reload Mod" en NMS.py.
* Funciona aunque NMS.py se inicie con la partida ya cargada.
* Textos en espanol o ingles (segun Windows, o el que se elija en el overlay). Las etiquetas de la
  ventana de NMS.py cambian de idioma con "Reload Mod".
* Muestra distancia (km/m) y rumbo hasta el destino; al llegar (<= 50 m) lo quita solo.
* Historial de los ultimos 10 destinos (el overlay los recorre con las flechas arriba/abajo).
* Datos guardados en %LOCALAPPDATA%\\OlympianPlanetCoords\\datos.json (sin permisos de admin).
* API HTTP (http://127.0.0.1:5000/OlympianPlanetCoords/...) para el overlay de la esquina
  superior izquierda, y controles alternativos en la ventana de pyMHF.

Hilos
-----
El servidor HTTP y la GUI de pyMHF corren en hilos propios: solo leen una "foto" (snapshot) ya
calculada y meten peticiones en una cola. Todo lo que toca el juego ocurre dentro del hook
cGcPlayerHUD.Update (hilo del juego).

Verificado en el juego por MightyZeus (v0.0.1b y v0.0.2, NMS 185034): coordenadas en vivo, marcador
con el metodo BaseBuildingHUD y distancias. La calibracion y el metodo ScanMarkerList se conservan
como respaldo.
"""

from __future__ import annotations

import ctypes
import json
import logging
import math
import os
import queue
import re
import tempfile
import time
import locale
import sys
from enum import IntEnum
from typing import Optional

from pymhf import Mod
from pymhf.core.hooking import on_key_pressed
from pymhf.core.http_api import endpoint
from pymhf.core.memutils import get_addressof, map_struct
from pymhf.gui.decorators import BOOLEAN, ENUM, STRING, gui_button

import nmspy.data.basic_types as basic
import nmspy.data.types as nms
from nmspy.common import gameData
from nmspy.data.enums import EnvironmentLocation, cGcMarkerType

logger = logging.getLogger("OlympianPlanetCoords")

MOD_VERSION = "0.0.3"
CREDITS = {
    "author": "MightyZeus, originario de Nicaragua",
    "copyright": "Copyright (c) 2026 MightyZeus",
    "github": "https://github.com/ImMightyZeus",
    "repo": "https://github.com/ImMightyZeus/Olympian-Planet-Coords",
    "paypal": "https://paypal.me/ImMightyZeus",
    "license": "GPL-3.0",
    "thanks": [
        "A mi querida esposa y mi amada familia, por permitirme seguir siendo un niño aun en mi adultez.",
        "Pangro Daer Barun D'Gross, por seguirme corrompiendo con videojuegos.",
        "Fenrir, porque aunque ya casi no juega, aún se presta a conversar sobre mis aventuras y desventuras.",
    ],
}
MARKER_POINT_SIZE = 0x2B0  # nms.cGcMarkerPoint._total_size_ (NMS.py 185034.0)
MARKER_TYPE_OFFSET = 0x291  # nms.cGcMarkerPoint.meType  (c_enum8)
# Tecla que pide al overlay abrir el campo de texto. F8 no se ofrece por defecto: la usa
# Imperial Galactic Map (IGM) para "agregar a favoritos". F12 queda fuera (captura de Steam).
HOTKEY_CHOICES = ("f5", "f6", "f7", "f8", "f9", "f10", "f11", "ninguna")
HOTKEY_DEFAULT = "f9"

# --------------------------------------------------------------------------------------------
# Idiomas (espanol / ingles)
# --------------------------------------------------------------------------------------------
LANGS = ("es", "en")


def system_lang() -> str:
    """Idioma por defecto: OLYMPIAN_LANG si existe; si no, el idioma de Windows; si no, el locale."""
    env = os.environ.get("OLYMPIAN_LANG", "").strip().lower()[:2]
    if env in LANGS:
        return env
    try:
        if sys.platform == "win32":
            lid = ctypes.windll.kernel32.GetUserDefaultUILanguage()
            return "es" if (lid & 0x3FF) == 0x0A else "en"
        loc = (locale.getlocale()[0] or os.environ.get("LANG", "")).lower()
        return "es" if loc.startswith("es") else "en"
    except Exception:
        return "es"


MESSAGES = {
    "waiting": {"es": "esperando al juego", "en": "waiting for the game"},
    "not_on_planet": {"es": "no estas en un planeta", "en": "you're not on a planet"},
    "arrived": {"es": "Llegaste a {name}", "en": "You reached {name}"},
    "dest_set": {"es": "Destino: {coords}", "en": "Destination: {coords}"},
    "dest_cleared": {"es": "Destino quitado", "en": "Destination cleared"},
    "no_dest_here": {"es": "no hay destino en este planeta", "en": "no destination on this planet"},
    "marker_error": {"es": "error al crear el marcador: {err}", "en": "could not create the marker: {err}"},
    "marker_rejected": {"es": "el juego rechazo el marcador; prueba el otro metodo",
                        "en": "the game rejected the marker; try the other method"},
    "error": {"es": "error: {err}", "en": "error: {err}"},
    "invalid_input": {"es": "Entrada invalida: {err}", "en": "Invalid input: {err}"},
    "out_of_range": {"es": "coordenadas fuera de rango", "en": "coordinates out of range"},
    "bad_hotkey": {"es": "tecla no valida", "en": "invalid key"},
    "bad_lang": {"es": "idioma no valido", "en": "invalid language"},
    "no_destination": {"es": "sin destino", "en": "no destination"},
    "marker_label": {"es": "Destino", "en": "Destination"},
    "parse_format": {"es": "formato: usa  +12.34, -56.78  (o 12,34; -56,78)",
                     "en": "format: use  +12.34, -56.78  (or 12,34; -56,78)"},
    "parse_lat": {"es": "latitud fuera de rango (-90..90)", "en": "latitude out of range (-90..90)"},
    "parse_lon": {"es": "longitud fuera de rango (-180..180)", "en": "longitude out of range (-180..180)"},
}


def tr(key: str, lang: str, **kw) -> str:
    entry = MESSAGES[key]
    return entry.get(lang, entry["es"]).format(**kw)


class CoordsError(ValueError):
    """Error de entrada con codigo traducible (MESSAGES[code])."""

    def __init__(self, code: str):
        super().__init__(tr(code, "es"))
        self.code = code

    def text(self, lang: str) -> str:
        return tr(self.code, lang)
ARRIVAL_METERS = 50.0  # a esta distancia (o menos) se considera que llegaste
ORBIT_FACTOR = 3.0  # en el espacio, "orbita" = a menos de 3 radios del centro del planeta
HISTORY_MAX = 10
STABLE_SECONDS = 1.0  # espera tras cargar / teletransportarse antes de colocar el marcador

# Ubicaciones "sobre el planeta".
PLANET_LOCATIONS = {
    EnvironmentLocation.Enum.PlanetOnFoot,
    EnvironmentLocation.Enum.PlanetInShip,
    EnvironmentLocation.Enum.PlanetInVehicle,
    EnvironmentLocation.Enum.Underwater,
    EnvironmentLocation.Enum.Cave,
    EnvironmentLocation.Enum.IndoorInBase,
}
# Ubicaciones "en el espacio" donde puede haber un planeta cerca (orbita).
SPACE_LOCATIONS = {EnvironmentLocation.Enum.Default}


# --------------------------------------------------------------------------------------------
# Matematica pura (sin dependencias del juego; se prueba aparte)
# --------------------------------------------------------------------------------------------
class LonConvention(IntEnum):
    """Como se arma la longitud a partir del vector unitario (x, y, z) con Y = "arriba"."""

    atan2_x_z = 0
    atan2_z_x = 1
    atan2_negx_z = 2
    atan2_negz_x = 3


def _norm(v):
    length = math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])
    if length < 1e-6:
        raise ValueError("vector nulo")
    return (v[0] / length, v[1] / length, v[2] / length), length


def vector_to_latlon(rel, convention: LonConvention, invert_lat: bool) -> tuple[float, float]:
    """Vector relativo al centro del planeta -> (lat, lon) en grados. lon en (-180, 180]."""
    (x, y, z), _ = _norm(rel)
    lat = math.degrees(math.asin(max(-1.0, min(1.0, y))))
    if invert_lat:
        lat = -lat
    if convention == LonConvention.atan2_x_z:
        lon = math.atan2(x, z)
    elif convention == LonConvention.atan2_z_x:
        lon = math.atan2(z, x)
    elif convention == LonConvention.atan2_negx_z:
        lon = math.atan2(-x, z)
    else:
        lon = math.atan2(-z, x)
    lon = math.degrees(lon)
    if lon <= -180.0:
        lon += 360.0
    return lat, lon


def latlon_to_unit(lat: float, lon: float, convention: LonConvention, invert_lat: bool):
    """Inversa exacta de vector_to_latlon (devuelve vector unitario)."""
    if invert_lat:
        lat = -lat
    la, lo = math.radians(lat), math.radians(lon)
    y, h = math.sin(la), math.cos(la)
    if convention == LonConvention.atan2_x_z:
        x, z = h * math.sin(lo), h * math.cos(lo)
    elif convention == LonConvention.atan2_z_x:
        z, x = h * math.sin(lo), h * math.cos(lo)
    elif convention == LonConvention.atan2_negx_z:
        x, z = -h * math.sin(lo), h * math.cos(lo)
    else:
        z, x = -h * math.sin(lo), h * math.cos(lo)
    return (x, y, z)


def surface_distance(lat1, lon1, lat2, lon2, radius) -> float:
    """Distancia sobre la superficie (gran circulo, haversine) en las unidades de `radius`."""
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dp, dl = p2 - p1, math.radians(lon2 - lon1)
    a = math.sin(dp / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dl / 2) ** 2
    return 2 * radius * math.asin(min(1.0, math.sqrt(a)))


def initial_bearing(lat1, lon1, lat2, lon2) -> float:
    """Rumbo inicial desde el punto 1 hacia el 2, en grados (0 = norte, 90 = este)."""
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dl = math.radians(lon2 - lon1)
    x = math.sin(dl) * math.cos(p2)
    y = math.cos(p1) * math.sin(p2) - math.sin(p1) * math.cos(p2) * math.cos(dl)
    return (math.degrees(math.atan2(x, y)) + 360.0) % 360.0


def relative_direction(origin, target, right, up, at, flip_forward=False) -> tuple[float, float]:
    """Giro (yaw) e inclinacion (pitch) en grados del objetivo visto desde `origin` con la base
    (right, up, at). yaw 0 = de frente, +90 = a la derecha, +-180 = detras; pitch + = arriba."""
    d = (target[0] - origin[0], target[1] - origin[1], target[2] - origin[2])

    def dot(a, b):
        return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]

    fwd = dot(d, at) * (-1.0 if flip_forward else 1.0)
    side = dot(d, right) * (-1.0 if flip_forward else 1.0)
    vert = dot(d, up)
    yaw = math.degrees(math.atan2(side, fwd))
    pitch = math.degrees(math.atan2(vert, math.hypot(side, fwd)))
    return yaw, pitch


_CARDINALS = ["N", "NE", "E", "SE", "S", "SO", "O", "NO"]


_CARDINALS_EN = ("N", "NE", "E", "SE", "S", "SW", "W", "NW")


def cardinal(bearing: float, lang: str = "es") -> str:
    names = _CARDINALS_EN if lang == "en" else _CARDINALS
    return names[int(((bearing % 360) + 22.5) // 45) % 8]


_NUM = r"([+-]?\d+(?:\.\d+)?)"
_COORD_DOT_RE = re.compile(rf"^{_NUM}\s*[,\s]\s*{_NUM}(?:\s*[,\s]\s*(.*))?$")
_NUM_COMMA = r"([+-]?\d+(?:[.,]\d+)?)"
_COORD_SEMI_RE = re.compile(rf"^{_NUM_COMMA}\s*;\s*{_NUM_COMMA}(?:\s*[;\s]\s*(.*))?$")


def parse_coords(text: str) -> tuple[float, float, str]:
    """'+12.34, -56.78 Mi base' -> (12.34, -56.78, 'Mi base'). Lanza ValueError si no es valido.

    Con punto decimal se separa con coma o espacio; con coma decimal se separa con ';'.
    """
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


# --------------------------------------------------------------------------------------------
# Datos persistentes (JSON en %LOCALAPPDATA%, sin permisos de administrador)
# --------------------------------------------------------------------------------------------
class MarkerMethod(IntEnum):
    ScanMarkerList = 0  # cGcScanManager.mMarkerList.TryAddMarker (persistente)
    BaseBuildingHUD = 1  # cGcBaseBuildingManager.AddHUDMarker (se re-emite cada frame)


class PosSource(IntEnum):
    Entorno = 0  # cGcPlayerEnvironment.mPlayerTM (sigue a la nave y al vehiculo)
    Cuerpo = 1  # cGcPlayer.mGraphicsMatrix (solo se mueve a pie)


def default_data_path() -> str:
    base = os.environ.get("LOCALAPPDATA") or os.path.join(os.path.expanduser("~"), ".local", "share")
    return os.path.join(base, "OlympianPlanetCoords", "datos.json")


DEFAULT_DATA = {
    "version": 1,
    "destinations": {},  # planet_key -> {"lat", "lon", "name"}
    "history": [],  # [{"lat", "lon", "name"}], mas reciente primero, max HISTORY_MAX
    "radii": {},  # planet_key -> distancia al centro medida en la superficie (radio real del planeta)
    "settings": {"convention": 0, "invert_lat": False, "planet_pos_is_local": False, "method": 1,
                 "pos_source": 0, "env_add_offset": False, "arrow_flip": False,
                 "hotkey": HOTKEY_DEFAULT, "lang": None},
}


def load_data(path: str) -> dict:
    data = json.loads(json.dumps(DEFAULT_DATA))
    try:
        with open(path, encoding="utf-8") as f:
            raw = json.load(f)
        if isinstance(raw.get("destinations"), dict):
            data["destinations"] = raw["destinations"]
        if isinstance(raw.get("history"), list):
            data["history"] = raw["history"][:HISTORY_MAX]
        if isinstance(raw.get("settings"), dict):
            data["settings"].update(raw["settings"])
        if isinstance(raw.get("radii"), dict):
            data["radii"] = raw["radii"]
    except FileNotFoundError:
        pass
    except Exception:
        logger.exception("datos.json danado; se empieza con datos vacios (se guarda copia .bak)")
        try:
            os.replace(path, path + ".bak")
        except OSError:
            pass
    return data


def save_data(path: str, data: dict) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    fd, tmp = tempfile.mkstemp(prefix="datos_", suffix=".tmp", dir=os.path.dirname(path))
    with os.fdopen(fd, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=1)
    os.replace(tmp, path)


def normalize_hotkey(value) -> str:
    key = str(value or "").strip().lower()
    return key if key in HOTKEY_CHOICES else HOTKEY_DEFAULT


def configured_hotkey(path: Optional[str] = None) -> str:
    """Tecla guardada en datos.json. Se lee al cargar el mod: NMS.py registra las teclas en ese
    momento, asi que un cambio se aplica con "Reload Mod"."""
    try:
        with open(path or default_data_path(), encoding="utf-8") as f:
            return normalize_hotkey((json.load(f).get("settings") or {}).get("hotkey"))
    except Exception:
        return HOTKEY_DEFAULT


def normalize_lang(value) -> str:
    v = str(value or "").strip().lower()[:2]
    return v if v in LANGS else system_lang()


def configured_lang(path: Optional[str] = None) -> str:
    """Idioma guardado en datos.json (o el del sistema). Las etiquetas de la ventana de NMS.py se
    fijan al cargar el mod: un cambio de idioma se aplica ahi con "Reload Mod"."""
    try:
        with open(path or default_data_path(), encoding="utf-8") as f:
            return normalize_lang((json.load(f).get("settings") or {}).get("lang"))
    except Exception:
        return system_lang()


HOTKEY_INPUT = configured_hotkey()
GUI_LANG = configured_lang()

_GUI_LABELS = {
    "coords": {"es": "Coordenadas", "en": "Coordinates"},
    "dest": {"es": "Destino", "en": "Destination"},
    "goto": {"es": "Ir a (lat, lon)", "en": "Go to (lat, lon)"},
    "set_dest": {"es": "Poner destino", "en": "Set destination"},
    "clear_dest": {"es": "Quitar destino", "en": "Clear destination"},
    "result": {"es": "Resultado", "en": "Result"},
    "cal_lon": {"es": "Cal. longitud", "en": "Lon. calib."},
    "cal_lat": {"es": "Cal. inv. latitud", "en": "Invert lat."},
    "cal_center": {"es": "Cal. centro local", "en": "Local centre"},
    "pos": {"es": "Posicion", "en": "Position"},
    "pos_offset": {"es": "Pos. + offset", "en": "Pos. + offset"},
    "arrow_flip": {"es": "Flecha inv.", "en": "Invert arrow"},
    "marker": {"es": "Marcador", "en": "Marker"},
    "hotkey": {"es": "Tecla (Reload Mod)", "en": "Key (Reload Mod)"},
    "lang": {"es": "Idioma (Reload Mod)", "en": "Language (Reload Mod)"},
    "credits": {"es": "Creditos", "en": "Credits"},
}


def gui_label(key: str) -> str:
    return _GUI_LABELS[key][GUI_LANG]


class LangChoice(IntEnum):
    Espanol = 0
    English = 1


class HotkeyChoice(IntEnum):
    F5 = 0
    F6 = 1
    F7 = 2
    F8 = 3
    F9 = 4
    F10 = 5
    F11 = 6
    Ninguna = 7


def push_history(history: list, lat: float, lon: float, name: str) -> list:
    entry = {"lat": round(lat, 2), "lon": round(lon, 2), "name": name}
    rest = [h for h in history if (h.get("lat"), h.get("lon")) != (entry["lat"], entry["lon"])]
    return ([entry] + rest)[:HISTORY_MAX]


# --------------------------------------------------------------------------------------------
# Mod
# --------------------------------------------------------------------------------------------
class OlympianPlanetCoords(Mod):
    __author__ = "MightyZeus (https://github.com/ImMightyZeus)"
    __description__ = "Coordenadas planetarias en tiempo real + destinos / Live planet coordinates + destinations"
    __version__ = MOD_VERSION

    def __init__(self, data_path: Optional[str] = None):
        super().__init__()
        self._data_path = data_path or default_data_path()
        self.data = load_data(self._data_path)
        self.data["settings"]["lang"] = normalize_lang(self.data["settings"].get("lang"))
        self._requests: "queue.Queue[tuple[str, tuple]]" = queue.Queue()
        self._snapshot: dict = {"ok": False, "on_planet": False, "in_orbit": False,
                                "reason": tr("waiting", normalize_lang(self.data["settings"].get("lang"))),
                                "lat": None, "lon": None}
        self._last_result: dict = {"ok": None, "message": "", "seq": 0}
        self._active: Optional[dict] = None  # marcador colocado ahora en el juego
        self._failed_key: Optional[tuple] = None  # evita reintentar cada frame si el juego rechaza
        self._ctx: Optional[dict] = None  # contexto del frame actual (posiciones, planeta)
        self._stable_key: Optional[str] = None
        self._stable_since = 0.0
        self._last_error_log = 0.0
        self._input_text = ""
        self.input_request_id = 0
        self._bb_manager_seen = False
        self._app_from_update = False

    # ---- ajustes ---------------------------------------------------------------------------
    @property
    def convention(self) -> LonConvention:
        return LonConvention(int(self.data["settings"].get("convention", 0)))

    @property
    def invert_lat(self) -> bool:
        return bool(self.data["settings"].get("invert_lat", False))

    @property
    def planet_pos_is_local(self) -> bool:
        return bool(self.data["settings"].get("planet_pos_is_local", False))

    @property
    def pos_source(self) -> PosSource:
        return PosSource(int(self.data["settings"].get("pos_source", 0)))

    @property
    def env_add_offset(self) -> bool:
        return bool(self.data["settings"].get("env_add_offset", False))

    @property
    def arrow_flip(self) -> bool:
        return bool(self.data["settings"].get("arrow_flip", False))

    @property
    def method(self) -> MarkerMethod:
        return MarkerMethod(int(self.data["settings"].get("method", 0)))

    @property
    def lang(self) -> str:
        return normalize_lang(self.data["settings"].get("lang"))

    def t(self, key: str, **kw) -> str:
        return tr(key, self.lang, **kw)

    def _save_lang(self, code: str):
        self.data["settings"]["lang"] = normalize_lang(code)
        self._save()

    @property
    def hotkey_saved(self) -> str:
        return normalize_hotkey(self.data["settings"].get("hotkey"))

    def _save_hotkey(self, key: str):
        # Solo se guarda: no hace falta quitar el marcador. Se aplica con "Reload Mod".
        self.data["settings"]["hotkey"] = normalize_hotkey(key)
        self._save()

    def _set_setting(self, key, value):
        self.data["settings"][key] = value
        self._remove_active()  # el marcador se recoloca con los ajustes nuevos
        self._save()

    def _save(self):
        try:
            save_data(self._data_path, self.data)
        except Exception:
            logger.exception("No se pudo guardar %s", self._data_path)

    def _set_result(self, ok: bool, message: str):
        self._last_result = {"ok": ok, "message": message, "seq": self._last_result.get("seq", 0) + 1}

    # ---- lectura del juego (hilo del juego) ------------------------------------------------
    def _read_context(self) -> dict:
        """Lee posiciones y planeta. Lanza LookupError si no hay planeta cerca."""
        player, env = gameData.player, gameData.player_environment
        if player is None or env is None:
            raise LookupError(self.t("waiting"))
        try:
            raw = env.meLocationStable  # pymhf c_enum32: int() directo falla, usar .value
            location = EnvironmentLocation.Enum(int(getattr(raw, "value", raw)))
        except (ValueError, TypeError):
            location = None
        gpos = player.mGraphicsMatrix.pos  # cuerpo del jugador (cTkPhysRelVec3); NO se mueve dentro de la nave
        goff = (gpos.offset.x, gpos.offset.y, gpos.offset.z)
        if self.pos_source == PosSource.Entorno:
            # cGcPlayerEnvironment.mPlayerTM: posicion real (a pie, en nave o en vehiculo)
            p = env.mPlayerTM.pos
            offset = goff if self.env_add_offset else (0.0, 0.0, 0.0)
            world = (p.x + offset[0], p.y + offset[1], p.z + offset[2])
        else:
            offset = goff
            world = (gpos.local.x + goff[0], gpos.local.y + goff[1], gpos.local.z + goff[2])
        c = env.mNearestPlanetPos
        center = (c.x, c.y, c.z)
        if self.planet_pos_is_local:
            center = (center[0] + offset[0], center[1] + offset[1], center[2] + offset[2])
        rel = (world[0] - center[0], world[1] - center[1], world[2] - center[2])
        _, dist_center = _norm(rel)
        planet_key = self._planet_key(env)
        # Radio real: la distancia al centro medida mientras estas en el planeta. (mfNearestPlanetSealevel
        # NO es el radio: en el juego vale ~120.) Se guarda por planeta para usarlo en orbita.
        radii = self.data.setdefault("radii", {})
        if location in PLANET_LOCATIONS:
            in_orbit = False
            radius = dist_center
            old = radii.get(planet_key)
            if not old or abs(old - dist_center) / old > 0.02:
                radii[planet_key] = round(dist_center, 1)
                self._save()
        elif location in SPACE_LOCATIONS and radii.get(planet_key) and dist_center <= ORBIT_FACTOR * radii[planet_key]:
            in_orbit = True
            radius = radii[planet_key]
        else:
            raise LookupError(self.t("not_on_planet"))
        tm = env.mPlayerTM
        basis = tuple((v.x, v.y, v.z) for v in (tm.right, tm.up, tm.at))
        return {"world": world, "offset": offset, "center": center, "rel": rel, "basis": basis,
                "dist_center": dist_center, "radius": radius, "in_orbit": in_orbit,
                "planet_key": planet_key, "location": location}

    @staticmethod
    def _raw_positions() -> dict:
        try:
            env, player = gameData.player_environment, gameData.player
            t, g = env.mPlayerTM.pos, player.mGraphicsMatrix.pos
            return {"env_mPlayerTM": [t.x, t.y, t.z],
                    "graphics_local": [g.local.x, g.local.y, g.local.z],
                    "graphics_offset": [g.offset.x, g.offset.y, g.offset.z],
                    "nearest_planet_pos": [env.mNearestPlanetPos.x, env.mNearestPlanetPos.y, env.mNearestPlanetPos.z],
                    "distance_from_planet": env.mfDistanceFromPlanet, "sealevel": env.mfNearestPlanetSealevel,
                    "inside_atmosphere": bool(env.mbInsidePlanetAtmosphere)}
        except Exception as e:
            return {"error": str(e)}

    @staticmethod
    def _planet_key(env) -> str:
        """Identificador del planeta: galaxia, region (voxel), sistema y numero de planeta."""
        try:
            ga = gameData.player_state.mLocation.GalacticAddress
            reality = gameData.player_state.mLocation.RealityIndex
            return (f"G{reality}:{ga.VoxelX}:{ga.VoxelY}:{ga.VoxelZ}:{ga.SolarSystemIndex}"
                    f":P{int(env.miNearestPlanetIndex)}")
        except Exception:
            return f"P{int(env.miNearestPlanetIndex)}"

    def _update(self):
        try:
            ctx = self._read_context()
        except LookupError as e:
            self._ctx = None
            self._stable_key = None
            self._snapshot = {"ok": True, "on_planet": False, "in_orbit": False, "reason": str(e),
                              "lat": None, "lon": None, "destination": None}
            return
        lat, lon = vector_to_latlon(ctx["rel"], self.convention, self.invert_lat)
        ctx["lat"], ctx["lon"] = lat, lon
        self._ctx = ctx
        now = time.monotonic()
        if ctx["planet_key"] != self._stable_key:
            self._stable_key, self._stable_since = ctx["planet_key"], now
        ctx["stable"] = (now - self._stable_since) >= STABLE_SECONDS

        dest = self.data["destinations"].get(ctx["planet_key"])
        dest_info = None
        if dest:
            dist = surface_distance(lat, lon, dest["lat"], dest["lon"], ctx["radius"])
            brg = initial_bearing(lat, lon, dest["lat"], dest["lon"])
            dest_info = {"lat": dest["lat"], "lon": dest["lon"], "name": dest.get("name", ""),
                         "distance_m": round(dist, 1), "bearing": round(brg), "cardinal": cardinal(brg, self.lang)}
            try:
                ux, uy, uz = latlon_to_unit(dest["lat"], dest["lon"], self.convention, self.invert_lat)
                c, r = ctx["center"], ctx["radius"]
                target = (c[0] + ux * r, c[1] + uy * r, c[2] + uz * r)
                right, up, at = ctx["basis"]
                yaw, pitch = relative_direction(ctx["world"], target, right, up, at, self.arrow_flip)
                dest_info["yaw"], dest_info["pitch"] = round(yaw, 1), round(pitch, 1)
            except Exception:
                pass
            if not ctx["in_orbit"] and ctx["stable"] and dist <= ARRIVAL_METERS:
                name = dest.get("name") or f"{dest['lat']:+.2f}, {dest['lon']:+.2f}"
                del self.data["destinations"][ctx["planet_key"]]
                self._remove_active()
                self._save()
                self._set_result(True, self.t("arrived", name=name))
                dest_info = None
        self._snapshot = {
            "ok": True, "on_planet": not ctx["in_orbit"], "in_orbit": ctx["in_orbit"], "reason": "",
            "lat": round(lat, 2), "lon": round(lon, 2), "planet_key": ctx["planet_key"],
            "destination": dest_info,
        }

    # ---- marcador en el juego (hilo del juego) ---------------------------------------------
    def _target_bigpos(self, lat: float, lon: float):
        ctx = self._ctx
        ux, uy, uz = latlon_to_unit(lat, lon, self.convention, self.invert_lat)
        r = ctx["radius"]  # en tierra: tu distancia al centro; en orbita: el radio guardado
        c, off = ctx["center"], ctx["offset"]
        tw = (c[0] + ux * r, c[1] + uy * r, c[2] + uz * r)
        big = basic.cTkPhysRelVec3()
        big.local = basic.Vector3f(tw[0] - off[0], tw[1] - off[1], tw[2] - off[2])
        big.offset = basic.Vector3f(*off)
        return big

    def _target_matrix(self, lat: float, lon: float):
        """cTkPhysRelMat34 para AddHUDMarker. Verificado en NMS.exe 185034: la funcion lee la
        posicion en +0x30 (local) y +0x40 (offset), o sea una matriz completa, no un cTkPhysRelVec3."""
        m = basic.cTkPhysRelMat34()
        m.right = basic.Vector3f(1, 0, 0)
        m.up = basic.Vector3f(0, 1, 0)
        m.at = basic.Vector3f(0, 0, 1)
        m.pos = self._target_bigpos(lat, lon)
        return m

    def _build_marker_point(self, lat: float, lon: float, name: str):
        buf = (ctypes.c_ubyte * MARKER_POINT_SIZE)()
        nms.cGcMarkerPoint.cGcMarkerPoint(ctypes.addressof(buf))  # constructor del juego
        pt = nms.cGcMarkerPoint.from_buffer(buf)
        pt.mPosition = self._target_bigpos(lat, lon)
        pt.mCenterOffset = basic.Vector3f(0, 0, 0)
        pt.mCustomName.set(name[:0x3F])
        pt.mCustomSubtitle.set(f"{lat:+.2f}, {lon:+.2f}"[:0x7F])
        buf[MARKER_TYPE_OFFSET] = int(cGcMarkerType.CustomMarker)
        return buf, pt

    def _remove_active(self):
        act, self._active = self._active, None
        self._failed_key = None
        if not act or act.get("buf") is None:
            return
        sim = gameData.simulation
        if sim is None:
            return
        try:
            pt = nms.cGcMarkerPoint.from_buffer(act["buf"])
            sim.mScanManager.mMarkerList.RemoveMarker(ctypes.byref(pt))
        except Exception:
            logger.exception("No se pudo quitar el marcador")

    def _sync_marker(self):
        """Hace que el marcador del juego coincida con el destino del planeta actual."""
        ctx = self._ctx
        desired = None
        if ctx is not None:
            d = self.data["destinations"].get(ctx["planet_key"])
            if d:
                desired = (ctx["planet_key"], d["lat"], d["lon"], d.get("name", ""), int(self.method))
        current = self._active["key"] if self._active else None
        if current is not None and current != desired:
            self._remove_active()
        if desired is None or self._active is not None or not ctx.get("stable"):
            return
        if desired == self._failed_key:
            return
        _, lat, lon, name, method = desired
        label = name or f"{self.t('marker_label')} {lat:+.2f},{lon:+.2f}"
        if method == MarkerMethod.ScanMarkerList:
            sim = gameData.simulation
            if sim is None:
                return
            try:
                buf, pt = self._build_marker_point(lat, lon, label)
                added = sim.mScanManager.mMarkerList.TryAddMarker(ctypes.byref(pt), True)
            except Exception as e:
                logger.exception("Fallo al crear el marcador")
                self._failed_key = desired
                self._set_result(False, self.t("marker_error", err=e))
                return
            # restype c_char -> bytes (b"\x00"/b"\x01"); None si la llamada fallo
            ok = added not in (b"", b"\x00") if isinstance(added, (bytes, bytearray)) else bool(added)
            if not ok:
                self._failed_key = desired
                self._set_result(False, self.t("marker_rejected"))
                return
            self._active = {"key": desired, "buf": buf}
        else:
            self._active = {"key": desired, "buf": None}  # se dibuja en on_base_building_update
        logger.info("Marcador activo: %s (%+.2f, %+.2f) metodo=%s", label, lat, lon, MarkerMethod(method).name)

    def _process_requests(self) -> bool:
        """Procesa la cola. Devuelve True si se proceso algo."""
        processed = False
        while True:
            try:
                kind, args = self._requests.get_nowait()
            except queue.Empty:
                return processed
            processed = True
            try:
                if kind == "place":
                    lat, lon, name = args
                    if self._ctx is None:
                        raise LookupError(self._snapshot.get("reason") or "no estas en un planeta")
                    key = self._ctx["planet_key"]
                    self.data["destinations"][key] = {"lat": round(lat, 2), "lon": round(lon, 2), "name": name}
                    self.data["history"] = push_history(self.data["history"], lat, lon, name)
                    self._save()
                    self._remove_active()  # reemplaza el destino anterior de este planeta
                    coords = f"{lat:+.2f}, {lon:+.2f}" + (f" {name}" if name else "")
                    self._set_result(True, self.t("dest_set", coords=coords))
                elif kind == "clear":
                    if self._ctx is None or self._ctx["planet_key"] not in self.data["destinations"]:
                        raise LookupError(self.t("no_dest_here"))
                    del self.data["destinations"][self._ctx["planet_key"]]
                    self._remove_active()
                    self._save()
                    self._set_result(True, self.t("dest_cleared"))
            except LookupError as e:
                self._set_result(False, str(e))
            except Exception as e:
                logger.exception("Fallo al procesar peticion")
                self._set_result(False, self.t("error", err=e))

    # ---- hooks del juego -------------------------------------------------------------------
    @nms.cGcApplication.Update.after
    def on_app_update(self, this):
        """NMS.py solo localiza cGcApplication en el primer cambio de estado del juego. Si NMS.py se
        inyecta con la partida ya cargada, ese cambio no llega y gameData queda vacio para siempre:
        aqui se toma de 'this' (cGcApplication::Update corre cada frame)."""
        if gameData.GcApplication is None:
            try:
                gameData.GcApplication = map_struct(get_addressof(this), nms.cGcApplication)
                self._app_from_update = True
                logger.info("cGcApplication localizado desde cGcApplication::Update")
            except Exception:
                if time.time() - self._last_error_log > 5:
                    self._last_error_log = time.time()
                    logger.exception("No se pudo localizar cGcApplication")

    @nms.cGcPlayerHUD.Update.after
    def on_hud_update(self, this, lfTimeStep):
        try:
            self._update()
            if self._process_requests():
                self._update()  # la foto refleja el destino nuevo en este mismo frame
            self._sync_marker()
        except Exception as e:  # nunca romper el frame del juego
            self._snapshot = {"ok": False, "on_planet": False, "in_orbit": False, "reason": self.t("error", err=e),
                              "lat": None, "lon": None, "destination": None}
            if time.time() - self._last_error_log > 5:
                self._last_error_log = time.time()
                logger.exception("Error en el ciclo del mod")

    @nms.cGcBaseBuildingManager.Update.after
    def on_base_building_update(self, this, lfTimeStep):
        self._bb_manager_seen = True
        act = self._active
        if not act or act["buf"] is not None or self._ctx is None:
            return
        try:
            _, lat, lon, name, _ = act["key"]
            big = self._target_matrix(lat, lon)
            label = basic.cTkFixedString0x40()
            label.set((name or f"{self.t('marker_label')} {lat:+.2f},{lon:+.2f}")[:0x3F])
            # NMS.py declara el argumento como puntero a cTkBigPos (cTkPhysRelVec3), pero el juego lee una
            # matriz completa: se pasa la matriz con el tipo de puntero que ctypes espera.
            ptr = ctypes.cast(ctypes.pointer(big), ctypes.POINTER(basic.cTkPhysRelVec3))
            this.contents.AddHUDMarker(int(cGcMarkerType.CustomMarker), ptr, 0, ctypes.byref(label))
        except Exception:
            if time.time() - self._last_error_log > 5:
                self._last_error_log = time.time()
                logger.exception("Error en AddHUDMarker")

    if HOTKEY_INPUT != "ninguna":

        @on_key_pressed(HOTKEY_INPUT)
        def request_input(self):
            self.input_request_id += 1

    # ---- API HTTP (overlay). Solo leen la foto / encolan -----------------------------------
    @property
    @endpoint(route="coords", method="GET")
    def http_coords(self):
        snap = dict(self._snapshot)
        snap["history"] = list(self.data["history"])
        snap["last_result"] = self._last_result
        snap["input_request_id"] = self.input_request_id
        snap["version"] = MOD_VERSION
        snap["hotkey"] = HOTKEY_INPUT  # la que esta activa ahora
        snap["hotkey_saved"] = self.hotkey_saved  # la elegida (se activa con Reload Mod)
        snap["lang"] = self.lang
        return snap

    @property
    @endpoint(route="debug", method="GET")
    def http_debug(self):
        ctx = self._ctx or {}
        return {
            "snapshot": self._snapshot,
            "settings": self.data["settings"],
            "data_path": self._data_path,
            "destinations": self.data["destinations"],
            "active_marker": bool(self._active),
            "base_building_manager_seen": self._bb_manager_seen,
            "app_from_update": self._app_from_update,
            "hotkey": HOTKEY_INPUT,
            "context": {k: (v.name if isinstance(v, IntEnum) else v) for k, v in ctx.items()},
            "raw": self._raw_positions(),
            "credits": CREDITS,
        }

    @endpoint(route="marker", method="PUT")
    def http_marker(self, lat: float, lon: float, name: str = ""):
        if not (-90 <= lat <= 90 and -180 <= lon <= 180):
            return {"queued": False, "message": self.t("out_of_range")}
        self._requests.put(("place", (float(lat), float(lon), name.strip()[:60])))
        try:
            from pymhf.core.utils import set_main_window_active

            set_main_window_active()  # devolver el foco al juego
        except Exception:
            pass
        return {"queued": True}

    @endpoint(route="clear", method="PUT")
    def http_clear(self, confirm: bool = True):
        self._requests.put(("clear", ()))
        return {"queued": True}

    @endpoint(route="hotkey", method="PUT")
    def http_hotkey(self, key: str):
        k = str(key).strip().lower()
        if k not in HOTKEY_CHOICES:
            return {"saved": False, "message": self.t("bad_hotkey")}
        self._save_hotkey(k)
        return {"saved": True, "hotkey": HOTKEY_INPUT, "hotkey_saved": k, "pending_reload": k != HOTKEY_INPUT}

    @endpoint(route="lang", method="PUT")
    def http_lang(self, code: str):
        c = str(code).strip().lower()
        if c not in LANGS:
            return {"saved": False, "message": self.t("bad_lang")}
        self._save_lang(c)
        return {"saved": True, "lang": c, "gui_lang": GUI_LANG}

    # ---- GUI de pyMHF (alternativa al overlay + calibracion) -------------------------------
    @property
    @STRING(gui_label("coords"))
    def gui_current(self):
        s = self._snapshot
        if s.get("lat") is None:
            return s.get("reason", "")
        return f"{s['lat']:+.2f}, {s['lon']:+.2f}" + (" (orbita)" if s.get("in_orbit") else "")

    @property
    @STRING(gui_label("dest"))
    def gui_destination(self):
        d = self._snapshot.get("destination")
        if not d:
            return self.t("no_destination")
        return f"{d['lat']:+.2f}, {d['lon']:+.2f}  {format_distance(d['distance_m'])} {d['cardinal']}"

    @property
    @STRING(gui_label("goto"))
    def gui_input(self):
        return self._input_text

    @gui_input.setter
    def gui_input(self, value: str):
        self._input_text = value

    @gui_button(gui_label("set_dest"))
    def gui_place(self):
        try:
            lat, lon, name = parse_coords(self._input_text)
        except ValueError as e:
            err = e.text(self.lang) if isinstance(e, CoordsError) else e
            self._set_result(False, self.t("invalid_input", err=err))
            return
        self._requests.put(("place", (lat, lon, name)))

    @gui_button(gui_label("clear_dest"))
    def gui_clear(self):
        self._requests.put(("clear", ()))

    @property
    @STRING(gui_label("result"))
    def gui_last(self):
        return self._last_result.get("message", "")

    @property
    @ENUM(gui_label("cal_lon"), LonConvention)
    def gui_convention(self):
        return self.convention

    @gui_convention.setter
    def gui_convention(self, value):
        if value is None:  # pyMHF pasa None si el nombre no existe
            return
        self._set_setting("convention", int(LonConvention(value)))

    @property
    @BOOLEAN(gui_label("cal_lat"))
    def gui_invert_lat(self):
        return self.invert_lat

    @gui_invert_lat.setter
    def gui_invert_lat(self, value: bool):
        self._set_setting("invert_lat", bool(value))

    @property
    @BOOLEAN(gui_label("cal_center"))
    def gui_planet_local(self):
        return self.planet_pos_is_local

    @gui_planet_local.setter
    def gui_planet_local(self, value: bool):
        self._set_setting("planet_pos_is_local", bool(value))

    @property
    @ENUM(gui_label("pos"), PosSource)
    def gui_pos_source(self):
        return self.pos_source

    @gui_pos_source.setter
    def gui_pos_source(self, value):
        if value is None:
            return
        self._set_setting("pos_source", int(PosSource(value)))

    @property
    @BOOLEAN(gui_label("pos_offset"))
    def gui_env_offset(self):
        return self.env_add_offset

    @gui_env_offset.setter
    def gui_env_offset(self, value: bool):
        self._set_setting("env_add_offset", bool(value))

    @property
    @BOOLEAN(gui_label("arrow_flip"))
    def gui_arrow_flip(self):
        return self.arrow_flip

    @gui_arrow_flip.setter
    def gui_arrow_flip(self, value: bool):
        self._set_setting("arrow_flip", bool(value))

    @property
    @ENUM(gui_label("marker"), MarkerMethod)
    def gui_method(self):
        return self.method

    @gui_method.setter
    def gui_method(self, value):
        if value is None:
            return
        self._set_setting("method", int(MarkerMethod(value)))

    @property
    @ENUM(gui_label("hotkey"), HotkeyChoice)
    def gui_hotkey(self):
        return HotkeyChoice(HOTKEY_CHOICES.index(self.hotkey_saved))

    @gui_hotkey.setter
    def gui_hotkey(self, value):
        if value is None:  # pyMHF pasa None si el nombre no existe
            return
        self._save_hotkey(HOTKEY_CHOICES[int(HotkeyChoice(value))])

    @property
    @ENUM(gui_label("lang"), LangChoice)
    def gui_lang(self):
        return LangChoice(LANGS.index(self.lang))

    @gui_lang.setter
    def gui_lang(self, value):
        if value is None:
            return
        self._save_lang(LANGS[int(LangChoice(value))])

    @property
    @STRING(gui_label("credits"))
    def gui_credits(self):
        return f"{CREDITS['author']} | {CREDITS['repo']} | {CREDITS['paypal']}"


def format_distance(meters: float) -> str:
    if meters >= 1000:
        return f"{meters / 1000:.1f} km"
    return f"{meters:.0f} m"
