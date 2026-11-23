"""Pruebas del mod v0.0.3 con pymhf 0.2.5 y nmspy 185034.0 REALES (solo se sustituyen las partes
exclusivas de Windows y las llamadas a funciones del juego, que se registran)."""
import ctypes
import importlib.util
import json
import math
import os
import random
import sys
import tempfile
import time

os.environ.setdefault("OLYMPIAN_LANG", "es")  # pruebas en espanol; el ingles se prueba aparte
import winstubs  # noqa: F401  (debe ir primero)

import nmspy.common as common
import nmspy.data.basic_types as basic
import nmspy.data.types as nms
from nmspy.data.enums import EnvironmentLocation, cGcMarkerType
from pymhf.core import hooking

HERE = os.path.dirname(__file__)
MOD_PATH = os.path.join(HERE, "..", "mod", "OlympianPlanetCoords.py")
OVERLAY_PATH = os.path.join(HERE, "..", "overlay", "olympian_overlay.py")


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m


mod = load(MOD_PATH, "OlympianPlanetCoords")
overlay = load(OVERLAY_PATH, "olympian_overlay")
mod.STABLE_SECONDS = 0.0  # en pruebas no esperamos 1 s

# ------------------------------------------------------------------ juego simulado
CALLS = []
TRY_ADD_RESULT = [b"\x01"]


def fake_call(self, *args, **kwargs):
    name = self._func.__qualname__
    CALLS.append((name, args))
    if name == "cGcMarkerList.TryAddMarker":
        return TRY_ADD_RESULT[0]
    if name == "cGcMarkerList.RemoveMarker":
        return 1
    return None


hooking.FunctionHook._call = fake_call

PLANET_CENTER = (1000.0, -2000.0, 500.0)
RADIUS = 3000.0  # nivel del mar
OFFSET = (100.0, 0.0, -50.0)


class FakeGame:
    def __init__(self):
        self.player = nms.cGcPlayer()
        self.env = nms.cGcPlayerEnvironment()
        self.sim = nms.cGcSimulation()
        self.state = nms.cGcPlayerState()
        self.env.mNearestPlanetPos = basic.Vector3f(*PLANET_CENTER)
        self.env.mPlayerTM.right = basic.Vector3f(1, 0, 0)
        self.env.mPlayerTM.up = basic.Vector3f(0, 1, 0)
        self.env.mPlayerTM.at = basic.Vector3f(0, 0, 1)
        self.env.mfNearestPlanetSealevel = 120.0  # en el juego real vale ~120 (no es el radio)
        self.set_planet(system=7, planet=2)
        self.set_location(EnvironmentLocation.Enum.PlanetOnFoot)

    def set_location(self, loc):
        ctypes.c_uint32.from_buffer(self.env, nms.cGcPlayerEnvironment.meLocationStable.offset).value = int(loc)

    def set_planet(self, system, planet):
        ga = self.state.mLocation.GalacticAddress
        ga.VoxelX, ga.VoxelY, ga.VoxelZ, ga.SolarSystemIndex = 10, -3, 400, system
        self.state.mLocation.RealityIndex = 0
        self.env.miNearestPlanetIndex = planet

    def put_player_at(self, unit, altitude=0.0):
        world = [PLANET_CENTER[i] + unit[i] * (RADIUS + altitude) for i in range(3)]
        p = self.player.mGraphicsMatrix.pos
        p.offset = basic.Vector3f(*OFFSET)
        p.local = basic.Vector3f(*(world[i] - OFFSET[i] for i in range(3)))
        self.env.mPlayerTM.pos = basic.Vector3f(*world)


GAME = FakeGame()
common.GameData.player = property(lambda s: GAME.player)
common.GameData.player_environment = property(lambda s: GAME.env)
common.GameData.simulation = property(lambda s: GAME.sim)
common.GameData.player_state = property(lambda s: GAME.state)

CONV = mod.LonConvention.atan2_x_z


def at(lat, lon, alt=0.0):
    GAME.put_player_at(mod.latlon_to_unit(lat, lon, CONV, False), alt)


def make_mod():
    path = os.path.join(tempfile.mkdtemp(), "OlympianPlanetCoords", "datos.json")
    m = mod.OlympianPlanetCoords(data_path=path)
    m.data["settings"]["method"] = 0  # la mayoria de pruebas usan el metodo de lista
    GAME.set_location(EnvironmentLocation.Enum.PlanetOnFoot)
    GAME.set_planet(7, 2)
    return m


def tick(m, n=1):
    for _ in range(n):
        m.on_hud_update(None, 0.016)


# ------------------------------------------------------------------ pruebas
def test_parsers_identicos():
    casos = ["+12.34, -56.78", "12.34 -56.78", "-0.5,179.99 Mi base", "12,34; -56,78", "1,5;2,5;Casa",
             "  +90, -180 ", "91, 0", "0, 181", "abc", "12", "12.3.4, 5", "-12.5,-170.25,Torre"]
    for c in casos:
        r = []
        for p in (mod.parse_coords, overlay.parse_coords):
            try:
                r.append(p(c))
            except ValueError as e:
                r.append(("ERR", str(e)))
        assert r[0] == r[1], (c, r)
    assert mod.parse_coords("-0.5,179.99 Mi base") == (-0.5, 179.99, "Mi base")
    assert mod.parse_coords("1,5;2,5;Casa") == (1.5, 2.5, "Casa")


def test_ida_y_vuelta_todas_las_convenciones():
    rnd = random.Random(1)
    for conv in mod.LonConvention:
        for inv in (False, True):
            for _ in range(300):
                lat, lon = rnd.uniform(-89.9, 89.9), rnd.uniform(-179.9, 179.9)
                u = mod.latlon_to_unit(lat, lon, conv, inv)
                lat2, lon2 = mod.vector_to_latlon(tuple(c * 1234.5 for c in u), conv, inv)
                assert abs(lat - lat2) < 1e-6 and abs(lon - lon2) < 1e-6


def test_distancia_y_rumbo():
    # 1 grado sobre el ecuador de un planeta de radio R = R*pi/180
    assert abs(mod.surface_distance(0, 0, 0, 1, 3000) - 3000 * math.pi / 180) < 1e-6
    assert round(mod.initial_bearing(0, 0, 10, 0)) == 0 and mod.cardinal(0) == "N"
    assert round(mod.initial_bearing(0, 0, 0, 10)) == 90 and mod.cardinal(90) == "E"
    assert round(mod.initial_bearing(0, 0, -10, 0)) == 180 and mod.cardinal(180) == "S"
    assert round(mod.initial_bearing(0, 0, 0, -10)) == 270 and mod.cardinal(270) == "O"
    assert mod.cardinal(45) == "NE" and mod.cardinal(315) == "NO" and mod.cardinal(359) == "N"
    assert mod.format_distance(1834) == "1.8 km" and mod.format_distance(42.4) == "42 m"
    assert overlay.format_distance(1834) == "1.8 km"
    assert overlay.seg_digits(7.2, 5) == ("!", "+", "7.20") and overlay.seg_digits(-120.33, 6) == ("", "-", "120.33")


def test_pymhf_detecta_hooks_gui_y_endpoints():
    m = make_mod()
    assert sorted(h._hook_func_name for h in m.hooks) == [
        "cGcApplication.Update", "cGcBaseBuildingManager.Update", "cGcPlayerHUD.Update"]
    assert len(m._hotkey_funcs) == 1
    assert [f._hotkey for f in m._hotkey_funcs] == [mod.HOTKEY_INPUT]
    assert sorted(f._api_endpoint.route for f in m._http_endpoints["GET"]) == ["/coords", "/debug"]
    put_routes = sorted(f._api_endpoint.route for f in m._http_endpoints["PUT"])
    assert put_routes == ["/clear", "/forget", "/hotkey", "/lang", "/marker"], put_routes
    labels = [getattr(getattr(w, "_widget_data", w), "label", None) for w in m._gui_widgets]
    for needed in ["Coordenadas", "Destino", "Ir a (lat, lon)", "Poner destino", "Quitar destino",
                   "Marcador", "Flecha inv.", "Tecla (Reload Mod)", "Idioma (Reload Mod)", "Creditos"]:
        assert needed in labels, (needed, labels)


def test_en_planeta_orbita_y_espacio():
    m = make_mod()
    at(23.45, -101.25)
    tick(m)
    assert abs(list(m.data["radii"].values())[0] - RADIUS) < 0.5, "guarda el radio real, no el sealevel"
    s = m.http_coords
    assert s["on_planet"] and not s["in_orbit"] and s["lat"] == 23.45 and s["lon"] == -101.25, s
    GAME.set_location(EnvironmentLocation.Enum.Default)
    at(10, 20, alt=4000)  # en el espacio, cerca: orbita
    tick(m)
    s = m.http_coords
    assert s["in_orbit"] and s["lat"] == 10 and s["lon"] == 20, s
    at(10, 20, alt=50000)  # lejos: no hay planeta
    tick(m)
    s = m.http_coords
    assert not s["in_orbit"] and s["lat"] is None and s["reason"] == "no estas en un planeta", s
    GAME.set_location(EnvironmentLocation.Enum.SpaceStation)
    at(10, 20)
    tick(m)
    assert m.http_coords["lat"] is None


def marker_world(buf):
    pt = nms.cGcMarkerPoint.from_buffer(buf)
    world = [getattr(pt.mPosition.local, a) + getattr(pt.mPosition.offset, a) for a in "xyz"]
    return tuple(world[i] - PLANET_CENTER[i] for i in range(3)), pt


def test_destino_marcador_distancia_y_llegada():
    m = make_mod()
    at(0, 0)
    CALLS.clear()
    assert m.http_marker(0, 1, "Torre") == {"queued": True}
    assert not any("TryAddMarker" in c[0] for c in CALLS), "no debe tocar el juego fuera de su hilo"
    tick(m)
    s = m.http_coords
    d = s["destination"]
    assert d and d["name"] == "Torre" and abs(d["distance_m"] - 3000 * math.pi / 180) < 0.2 and d["cardinal"] == "E", s
    assert s["last_result"]["ok"] and s["history"][0] == {"lat": 0.0, "lon": 1.0, "name": "Torre"}
    assert [c[0] for c in CALLS].count("cGcMarkerList.TryAddMarker") == 1
    rel, pt = marker_world(m._active["buf"])
    lat, lon = mod.vector_to_latlon(rel, CONV, False)
    assert abs(lat) < 1e-3 and abs(lon - 1) < 1e-3
    assert m._active["buf"][0x291] == int(cGcMarkerType.CustomMarker) and pt.mCustomName.value == b"Torre"
    tick(m, 5)
    assert [c[0] for c in CALLS].count("cGcMarkerList.TryAddMarker") == 1, "no debe duplicar el marcador"
    # se guarda en disco
    saved = json.load(open(m._data_path, encoding="utf-8"))
    assert list(saved["destinations"].values()) == [{"lat": 0.0, "lon": 1.0, "name": "Torre"}]
    # caminar hasta 40 m del destino -> llegada
    deg40 = math.degrees(40 / 3000)
    at(0, 1 - deg40)
    CALLS.clear()
    tick(m)
    s = m.http_coords
    assert s["destination"] is None and s["last_result"]["message"] == "Llegaste a Torre", s
    assert "cGcMarkerList.RemoveMarker" in [c[0] for c in CALLS] and m._active is None
    assert json.load(open(m._data_path, encoding="utf-8"))["destinations"] == {}
    assert s["history"][0]["name"] == "Torre", "el historial se conserva"


def test_memoria_por_planeta():
    m = make_mod()
    at(5, 5)
    m.http_marker(20, 30, "Lago")
    tick(m)
    key_a = m.http_coords["planet_key"]
    assert m._active is not None
    # otro planeta del mismo sistema
    GAME.set_planet(7, 3)
    CALLS.clear()
    tick(m)
    s = m.http_coords
    assert s["planet_key"] != key_a and s["destination"] is None, s
    assert "cGcMarkerList.RemoveMarker" in [c[0] for c in CALLS] and m._active is None
    m.http_marker(-40, -60, "Cueva")
    tick(m)
    assert m.http_coords["destination"]["name"] == "Cueva"
    assert set(m.data["destinations"]) == {key_a, m.http_coords["planet_key"]}, "no borra el del otro planeta"
    # vuelvo al primero: reaparece su destino y su marcador
    GAME.set_planet(7, 2)
    CALLS.clear()
    tick(m)
    s = m.http_coords
    assert s["destination"]["name"] == "Lago" and "cGcMarkerList.TryAddMarker" in [c[0] for c in CALLS]
    # nuevo destino en el mismo planeta reemplaza al anterior
    m.http_marker(1, 1, "Nuevo")
    tick(m)
    assert m.data["destinations"][key_a]["name"] == "Nuevo" and len(m.data["destinations"]) == 2
    # reinicio del juego: se recupera desde disco
    m2 = mod.OlympianPlanetCoords(data_path=m._data_path)
    tick(m2)
    assert m2.http_coords["destination"]["name"] == "Nuevo"
    # quitar destino solo de este planeta
    m2.http_clear()
    tick(m2)
    assert m2.http_coords["destination"] is None and len(m2.data["destinations"]) == 1


def test_destino_desde_orbita():
    m = make_mod()
    GAME.set_location(EnvironmentLocation.Enum.Default)
    at(10, 10, alt=3000)
    tick(m)
    assert m.http_coords["lat"] is None, "sin haber estado en el planeta no se conoce su radio"
    GAME.set_location(EnvironmentLocation.Enum.PlanetOnFoot)
    at(0, 0)
    tick(m)  # aprende el radio del planeta
    GAME.set_location(EnvironmentLocation.Enum.Default)
    at(10, 10, alt=3000)
    m.http_marker(12, 14, "Base")
    tick(m)
    s = m.http_coords
    assert s["in_orbit"] and s["destination"]["name"] == "Base" and m._active is not None
    rel, _ = marker_world(m._active["buf"])
    assert abs(math.sqrt(sum(c * c for c in rel)) - RADIUS) < 0.05, "en orbita el marcador va a nivel del mar"
    # cerca en orbita no cuenta como llegada
    at(12, 14, alt=3000)
    tick(m)
    assert m.http_coords["destination"] is not None


def test_distancia_lejana_no_es_llegada():
    """Bug real v0.0.1: con el radio mal (120) un destino a 52 grados salia a 110 m."""
    m = make_mod()
    at(-3.59, 64.56)
    m.http_marker(20, 17, "lejos")
    tick(m)
    d = m.http_coords["destination"]
    expected = mod.surface_distance(-3.59, 64.56, 20, 17, RADIUS)
    assert d and abs(d["distance_m"] - expected) < 1 and d["distance_m"] > 2000, d
    m.http_marker(-3.59, 64.60, "cerca")  # ~2 m con R=3000 -> llegada
    tick(m)
    assert m.http_coords["last_result"]["message"] == "Llegaste a cerca"


def test_direccion_relativa_pura():
    R, U, A = (1, 0, 0), (0, 1, 0), (0, 0, 1)
    o = (0, 0, 0)
    assert mod.relative_direction(o, (0, 0, 10), R, U, A) == (0.0, 0.0)  # de frente
    y, p = mod.relative_direction(o, (10, 0, 0), R, U, A)
    assert round(y) == 90 and round(p) == 0  # a la derecha
    y, _ = mod.relative_direction(o, (0, 0, -10), R, U, A)
    assert abs(round(y)) == 180  # detras
    _, p = mod.relative_direction(o, (0, 10, 10), R, U, A)
    assert round(p) == 45  # arriba
    y, _ = mod.relative_direction(o, (0, 0, 10), R, U, A, flip_forward=True)
    assert abs(round(y)) == 180  # con "Flecha inv." el frente es -at


def test_destino_incluye_giro_e_inclinacion():
    m = make_mod()
    at(0, 0)  # en (0,0) con CONV atan2_x_z el jugador esta en +z del centro
    m.http_marker(0, 10, "E")
    tick(m)
    d = m.http_coords["destination"]
    assert d["yaw"] is not None and d["pitch"] is not None, d
    assert d["yaw"] > 0, "el destino al este queda a la derecha (+x) con la base de prueba"


def test_historial_maximo_10_sin_duplicados():
    m = make_mod()
    at(0, 0)
    for i in range(13):
        m.http_marker(i, i, f"P{i}")
    m.http_marker(5, 5, "P5 otra vez")
    tick(m)
    h = m.http_coords["history"]
    assert len(h) == 10 and h[0] == {"lat": 5.0, "lon": 5.0, "name": "P5 otra vez"}
    assert [x["lat"] for x in h].count(5.0) == 1


def test_cruz_de_la_lista_quita_del_historial_y_del_destino():
    """Cruz de la lista de destinos (PUT /forget): quita el punto de la lista y, si es el destino del
    planeta actual, tambien el destino y su marcador. Los destinos de otros planetas no se tocan."""
    m = make_mod()
    at(0, 0)
    m.http_marker(10, 20, "A")
    m.http_marker(30.004, -40, "B")
    tick(m, 120)  # el marcador sale cuando la posicion es estable
    assert m.http_coords["destination"]["name"] == "B" and m._active
    # punto que no es el destino activo: solo sale de la lista
    assert m.http_forget(10, 20) == {"queued": True}
    tick(m)
    s = m.http_coords
    assert [h["name"] for h in s["history"]] == ["B"] and s["destination"]["name"] == "B", s
    assert s["last_result"]["ok"] is True and s["last_result"]["message"] == "Quitado de la lista: +10.00, +20.00"
    # el destino activo (redondeo a 2 decimales): sale de la lista y deja de ser destino
    m.http_forget(30.0, -40.001)
    tick(m)
    s = m.http_coords
    assert s["history"] == [] and s["destination"] is None and not m._active, s
    assert s["last_result"]["message"] == "Quitado de la lista y como destino: +30.00, -40.00"
    assert mod.load_data(m._data_path)["history"] == []
    # un punto que no esta en la lista
    m.http_forget(1, 1)
    tick(m)
    assert m.http_coords["last_result"] == {"ok": False, "message": "ese punto no esta en la lista",
                                            "seq": m.http_coords["last_result"]["seq"]}
    assert m.http_forget(91, 0)["queued"] is False
    # fuera del planeta: se puede quitar de la lista, el destino guardado del planeta no se toca
    m.http_marker(5, 5, "C")
    tick(m)
    GAME.set_location(EnvironmentLocation.Enum.SpaceStation)
    tick(m)
    m.http_forget(5, 5)
    tick(m)
    assert m.http_coords["history"] == []
    assert m.http_coords["last_result"]["message"] == "Quitado de la lista: +5.00, +5.00"
    assert any(d.get("name") == "C" for d in m.data["destinations"].values())
    GAME.set_location(EnvironmentLocation.Enum.PlanetOnFoot)


def test_rechazo_del_juego_sin_reintentos_infinitos():
    m = make_mod()
    at(0, 0)
    TRY_ADD_RESULT[0] = b"\x00"
    try:
        CALLS.clear()
        m.http_marker(1, 2, "")
        tick(m, 10)
        res = m.http_coords["last_result"]
        assert res["ok"] is False and "rechazo" in res["message"], res
        assert [c[0] for c in CALLS].count("cGcMarkerList.TryAddMarker") == 1
        assert m.http_coords["destination"] is not None, "el destino se mantiene para mostrar distancia"
    finally:
        TRY_ADD_RESULT[0] = b"\x01"


def test_fuera_de_planeta_rechaza():
    m = make_mod()
    GAME.set_location(EnvironmentLocation.Enum.SpaceStation)
    m.http_marker(1, 2, "")
    tick(m)
    res = m.http_coords["last_result"]
    assert res["ok"] is False and res["message"] == "no estas en un planeta", res
    assert m.data["destinations"] == {}


def test_metodo_hud():
    m = make_mod()
    m.gui_method = mod.MarkerMethod.BaseBuildingHUD
    at(0, 0)
    m.http_marker(45, 90, "Norte")
    tick(m)
    CALLS.clear()
    m.on_base_building_update(ctypes.pointer(nms.cGcBaseBuildingManager()), 0.016)
    calls = [c for c in CALLS if c[0] == "cGcBaseBuildingManager.AddHUDMarker"]
    assert len(calls) == 1, CALLS
    args = calls[0][1]
    mat = ctypes.cast(args[2], ctypes.POINTER(basic.cTkPhysRelMat34)).contents
    assert mat.right.x == 1 and mat.up.y == 1 and mat.at.z == 1
    big = mat.pos
    world = [getattr(big.local, a) + getattr(big.offset, a) for a in "xyz"]
    lat, lon = mod.vector_to_latlon(tuple(world[i] - PLANET_CENTER[i] for i in range(3)), CONV, False)
    assert abs(lat - 45) < 1e-3 and abs(lon - 90) < 1e-3 and args[4]._obj.value == b"Norte"
    assert json.load(open(m._data_path, encoding="utf-8"))["settings"]["method"] == 1


def test_layout_matriz_addhudmarker():
    """En NMS.exe 185034 AddHUDMarker lee [r8+0x30] y [r8+0x40] (desensamblado verificado)."""
    assert basic.cTkPhysRelMat34.pos.offset == 0x30
    assert basic.cTkPhysRelMat34.pos.offset + basic.cTkPhysRelVec3.offset.offset == 0x40


def test_fuente_de_posicion_sigue_a_la_nave():
    """Bug real v0.0.1a: dentro de la nave mGraphicsMatrix no se mueve; mPlayerTM si."""
    m = make_mod()
    at(0, 0)
    tick(m)
    # simula volar en nave: solo cambia el entorno, el cuerpo queda quieto
    GAME.set_location(EnvironmentLocation.Enum.PlanetInShip)
    u = mod.latlon_to_unit(5, 7, CONV, False)
    GAME.env.mPlayerTM.pos = basic.Vector3f(*(PLANET_CENTER[i] + u[i] * RADIUS for i in range(3)))
    tick(m)
    assert (m.http_coords["lat"], m.http_coords["lon"]) == (5.0, 7.0), m.http_coords
    m.gui_pos_source = mod.PosSource.Cuerpo
    tick(m)
    assert (m.http_coords["lat"], m.http_coords["lon"]) == (0.0, 0.0)
    assert "env_mPlayerTM" in m.http_debug["raw"]


def test_gui_y_calibracion():
    m = make_mod()
    at(0, 0)
    m.gui_input = "12.5, -45 Casa"
    m.gui_place()
    tick(m)
    assert m.http_coords["destination"]["name"] == "Casa" and "12.50" in m.gui_destination
    m.gui_input = "basura"
    m.gui_place()
    assert "Entrada invalida" in m.gui_last
    m.gui_convention = mod.LonConvention.atan2_negx_z
    assert m.convention == mod.LonConvention.atan2_negx_z
    m.gui_convention = None  # pyMHF pasa None si el nombre no existe
    assert m.convention == mod.LonConvention.atan2_negx_z
    assert "ImMightyZeus" in m.gui_credits and "paypal.me/ImMightyZeus" in m.gui_credits


def test_datos_danados_no_rompen():
    d = tempfile.mkdtemp()
    p = os.path.join(d, "datos.json")
    open(p, "w").write("{esto no es json")
    m = mod.OlympianPlanetCoords(data_path=p)
    assert m.data["destinations"] == {} and os.path.exists(p + ".bak")


def test_http_real_fastapi():
    from fastapi.testclient import TestClient
    from pymhf.core.http_api import CleanableAPIRouter, create_app

    m = make_mod()
    app = create_app()
    r = CleanableAPIRouter(m._mod_name, prefix=f"/{m._mod_name}")
    r.load_routes(m._http_endpoints)
    app.include_router(r)
    c = TestClient(app)
    at(-12.34, 56.78)
    tick(m)
    j = c.get("/OlympianPlanetCoords/coords").json()
    assert j["lat"] == -12.34 and j["lon"] == 56.78 and j["version"] == "0.0.3", j
    put = c.put("/OlympianPlanetCoords/marker", params={"lat": "1.5", "lon": "-2.5", "name": "X"})
    assert put.json() == {"queued": True}
    assert c.put("/OlympianPlanetCoords/marker", params={"lat": "100", "lon": "0"}).json()["queued"] is False
    assert c.put("/OlympianPlanetCoords/marker", params={"lat": "abc", "lon": "0"}).status_code == 422
    tick(m)
    j = c.get("/OlympianPlanetCoords/coords").json()
    assert j["destination"]["name"] == "X" and j["history"][0]["name"] == "X", j
    dbg = c.get("/OlympianPlanetCoords/debug").json()
    assert dbg["credits"]["paypal"] == "https://paypal.me/ImMightyZeus"
    assert c.put("/OlympianPlanetCoords/clear").json() == {"queued": True}
    tick(m)
    assert c.get("/OlympianPlanetCoords/coords").json()["destination"] is None
    # tecla
    j = c.get("/OlympianPlanetCoords/coords").json()
    assert j["hotkey"] == mod.HOTKEY_INPUT and j["hotkey_saved"] == "f9", j
    r = c.put("/OlympianPlanetCoords/hotkey", params={"key": "F7"}).json()
    assert r == {"saved": True, "hotkey": mod.HOTKEY_INPUT, "hotkey_saved": "f7",
                 "pending_reload": mod.HOTKEY_INPUT != "f7"}, r
    assert c.put("/OlympianPlanetCoords/hotkey", params={"key": "f12"}).json()["saved"] is False
    assert c.get("/OlympianPlanetCoords/coords").json()["hotkey_saved"] == "f7"
    with open(m._data_path, encoding="utf-8") as f:
        assert json.load(f)["settings"]["hotkey"] == "f7"


def test_tecla_por_defecto_f9_y_configurable():
    """F8 la usa Imperial Galactic Map: por defecto F9. La tecla se lee de datos.json al cargar."""
    assert mod.HOTKEY_DEFAULT == "f9"
    assert mod.normalize_hotkey(None) == "f9" and mod.normalize_hotkey("F11") == "f11"
    assert mod.normalize_hotkey("f12") == "f9" and mod.normalize_hotkey("ctrl+f8") == "f9"
    base = tempfile.mkdtemp()
    path = os.path.join(base, "OlympianPlanetCoords", "datos.json")
    assert mod.configured_hotkey(path) == "f9"  # sin archivo
    old = os.environ.get("LOCALAPPDATA")
    try:
        os.environ["LOCALAPPDATA"] = base
        for key, n_funcs in (("f6", 1), ("ninguna", 0)):
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "w", encoding="utf-8") as f:
                json.dump({"settings": {"hotkey": key}}, f)
            m2 = load(MOD_PATH, f"OPC_{key}")  # "Reload Mod" vuelve a importar el archivo
            assert m2.HOTKEY_INPUT == key
            inst = m2.OlympianPlanetCoords(data_path=path)
            assert len(inst._hotkey_funcs) == n_funcs
            assert [f._hotkey for f in inst._hotkey_funcs] == ([key] if n_funcs else [])
            assert inst.gui_hotkey == m2.HotkeyChoice(m2.HOTKEY_CHOICES.index(key))
            inst.gui_hotkey = m2.HotkeyChoice.F10
            assert inst.hotkey_saved == "f10" and m2.configured_hotkey(path) == "f10"
            inst.gui_hotkey = None  # pyMHF manda None si no reconoce el nombre: no cambia nada
            assert inst.hotkey_saved == "f10"
    finally:
        if old is None:
            os.environ.pop("LOCALAPPDATA", None)
        else:
            os.environ["LOCALAPPDATA"] = old


def test_localiza_aplicacion_si_se_inyecta_con_partida_cargada():
    """Con la partida ya cargada NMS.py no ve el cambio de estado y gameData.GcApplication queda en
    None: el hook de cGcApplication::Update lo arma desde el puntero global a cGcApplication::Data
    (el 'this' de Update no sirve: Update no usa rcx)."""
    data = nms.cGcApplication.Data()
    off = (nms.cGcApplication.Data.mSimulation.offset + nms.cGcSimulation.mEnvironment.offset
           + nms.cGcEnvironment.mPlayerEnvironment.offset + nms.cGcPlayerEnvironment.meLocationStable.offset)
    ctypes.c_uint32.from_buffer(data, off).value = int(EnvironmentLocation.Enum.PlanetOnFoot)
    glob = ctypes.c_uint64(ctypes.addressof(data))
    base = ctypes.addressof(glob) - mod.APP_DATA_GLOBAL_RVA
    old_read, old_base = mod._read_mem, mod._module_base
    mod._read_mem = lambda addr, size: ctypes.string_at(addr, size)
    mod._module_base = lambda: base
    prev = common.gameData.GcApplication
    try:
        assert mod.locate_app_data() == ctypes.addressof(data)
        m = make_mod()
        common.gameData.GcApplication = None
        m.on_app_update(ctypes.c_void_p(0x1234))  # 'this' de Update: se ignora
        app = common.gameData.GcApplication
        assert app is not None and m._app_from_update is True
        assert ctypes.addressof(app.mpData.contents) == ctypes.addressof(data)
        # (en estas pruebas gameData.simulation esta sustituido por el juego simulado: se mira via mpData)
        sim = app.mpData.contents.mSimulation
        assert ctypes.addressof(sim) == ctypes.addressof(data.mSimulation)
        loc = sim.mEnvironment.mPlayerEnvironment.meLocationStable
        assert int(getattr(loc, "value", loc)) == int(EnvironmentLocation.Enum.PlanetOnFoot)
        # si ya existe, no lo toca
        m._app_next_try = 0
        m.on_app_update(None)
        assert common.gameData.GcApplication is app
        # valores que no son de una partida: no se acepta
        ctypes.c_uint32.from_buffer(data, off).value = 99
        assert mod.locate_app_data() is None
        glob.value = 0
        assert mod.locate_app_data() is None
        mod._read_mem = lambda addr, size: None  # memoria ilegible
        assert mod.locate_app_data() is None
        common.gameData.GcApplication = None
        m2 = make_mod()
        m2.on_app_update(None)
        assert common.gameData.GcApplication is None and m2._app_from_update is False
    finally:
        common.gameData.GcApplication = prev
        mod._read_mem, mod._module_base = old_read, old_base


def test_argumentos_convertibles_por_ctypes():
    """Cada llamada registrada al juego pasa la conversion de tipos de ctypes con la firma real."""
    from pymhf.core.hooking import _get_funcdef

    m = make_mod()
    CALLS.clear()
    at(5, 5)
    m.http_marker(10, 10, "A")
    tick(m)
    m.http_clear()
    tick(m)
    m.gui_method = mod.MarkerMethod.BaseBuildingHUD
    m.http_marker(10, 10, "A")
    tick(m)
    m.on_base_building_update(ctypes.pointer(nms.cGcBaseBuildingManager()), 0.016)
    lookup = {
        "cGcMarkerPoint.cGcMarkerPoint": nms.cGcMarkerPoint.cGcMarkerPoint,
        "cGcMarkerList.TryAddMarker": nms.cGcMarkerList.TryAddMarker,
        "cGcMarkerList.RemoveMarker": nms.cGcMarkerList.RemoveMarker,
        "cGcBaseBuildingManager.AddHUDMarker": nms.cGcBaseBuildingManager.AddHUDMarker,
    }
    seen = set()
    for name, args in CALLS:
        fd = _get_funcdef(lookup[name]._func)
        flat = fd.flatten(*args)
        assert len(flat) == len(fd.arg_types), (name, flat, fd.arg_types)
        for t, a in zip(fd.arg_types, flat, strict=True):
            t.from_param(a)
        seen.add(name)
    assert seen == set(lookup), seen


def test_idioma_ruso():
    """Ruso: mensajes, puntos cardinales y /lang; la ventana de NMS.py sigue en ingles (sin cirilico)."""
    assert mod.LANGS == ("es", "en", "ru")
    assert [mod.cardinal(b, "ru") for b in range(0, 360, 45)] == ["С", "СВ", "В", "ЮВ", "Ю", "ЮЗ", "З", "СЗ"]
    for key, entry in mod.MESSAGES.items():
        assert entry.get("ru"), key
        # mismos campos {..} que en espanol
        import string
        fields = lambda t: sorted(f for _, f, _, _ in string.Formatter().parse(t) if f)  # noqa: E731
        assert fields(entry["ru"]) == fields(entry["es"]), key
    assert mod.normalize_lang("RU") == "ru" and mod.normalize_lang("fr") in ("es", "en", "ru")
    old = mod.GUI_LANG
    try:
        mod.GUI_LANG = "ru"
        assert mod.gui_label("coords") == "Coordinates"
    finally:
        mod.GUI_LANG = old
    m = make_mod()
    assert m.http_lang("ru") == {"saved": True, "lang": "ru", "gui_lang": mod.GUI_LANG}
    assert m.lang == "ru" and int(m.gui_lang) == int(mod.LangChoice.Russian)
    at(10.0, 0.0)
    tick(m)
    m.http_marker(10.0, 10.0, "Башня")
    tick(m)
    s = m.http_coords
    assert s["lang"] == "ru" and s["destination"]["cardinal"] == "В", s
    assert s["last_result"]["message"] == "Цель: +10.00, +10.00 Башня", s["last_result"]
    m.http_lang("es")


def test_idioma_ingles():
    """Mensajes del mod en ingles al elegirlo (desde el overlay o la ventana de NMS.py)."""
    assert mod.cardinal(270, "en") == "W" and mod.cardinal(315, "en") == "NW" and mod.cardinal(225) == "SO"
    m = make_mod()
    assert m.lang == "es" and m.http_coords["lang"] == "es"
    from fastapi.testclient import TestClient
    from pymhf.core.http_api import CleanableAPIRouter, create_app

    app = create_app()
    r = CleanableAPIRouter(m._mod_name, prefix=f"/{m._mod_name}")
    r.load_routes(m._http_endpoints)
    app.include_router(r)
    c = TestClient(app)
    assert c.put("/OlympianPlanetCoords/lang", params={"code": "fr"}).json()["saved"] is False
    r = c.put("/OlympianPlanetCoords/lang", params={"code": "en"}).json()
    assert r == {"saved": True, "lang": "en", "gui_lang": mod.GUI_LANG}, r
    with open(m._data_path, encoding="utf-8") as f:
        assert json.load(f)["settings"]["lang"] == "en"
    at(10.0, 0.0)
    tick(m)
    m.http_marker(10.0, 10.0, "Tower")
    tick(m)
    s = m.http_coords
    assert s["lang"] == "en" and s["destination"]["cardinal"] == "E", s
    assert s["last_result"]["message"].startswith("Destination: +10.00, +10.00 Tower"), s["last_result"]
    at(10.0, 10.0)
    tick(m)
    tick(m)
    assert m.http_coords["last_result"]["message"] == "You reached Tower"
    m.gui_input = "95, 10"
    m.gui_place()
    assert m.gui_last == "Invalid input: latitude out of range (-90..90)", m.gui_last
    m.gui_lang = mod.LangChoice.Espanol
    assert m.lang == "es"
    try:
        mod.parse_coords("hola")
        raise AssertionError("debe fallar")
    except mod.CoordsError as e:
        assert e.code == "parse_format" and "formato" in str(e) and e.text("en").startswith("format:")


if __name__ == "__main__":
    fails = 0
    t0 = time.time()
    for name, fn in list(globals().items()):
        if name.startswith("test_"):
            try:
                fn()
                print(f"OK   {name}")
            except Exception:
                fails += 1
                import traceback

                print(f"FAIL {name}")
                traceback.print_exc()
    print("FALLOS:", fails, f"({time.time() - t0:.1f}s)")
    sys.exit(1 if fails else 0)

