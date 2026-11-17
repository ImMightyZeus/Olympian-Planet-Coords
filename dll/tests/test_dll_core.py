"""Pruebas del nucleo del DLL contra la version Python v0.0.3 (la referencia probada en el juego).

Tests for the DLL core against the Python v0.0.3 version (the reference tested in game).

Compila dll/tests/core_harness.cpp con g++ (Linux) y compara:
  * la matematica (lat/lon, distancia, rumbo, cardinal, giro/inclinacion) con las funciones Python;
  * las respuestas /coords del DLL con lo que calcula la version Python para el mismo jugador;
  * el formato de datos.json en ambos sentidos (Python <-> DLL);
  * el servidor HTTP con el cliente real del overlay (ModClient).
Si existe wine y mingw, tambien compila el banco para Windows y prueba ReadGameView con
estructuras armadas en memoria con los desplazamientos reales (modo fakegame).

Uso: python dll/tests/test_dll_core.py   (desde la raiz del repositorio, con nmspy instalado)

Olympian Planet Coords — Copyright © 2026 MightyZeus · GPL-3.0 License
"""

import importlib.util
import json
import math
import os
import random
import shutil
import subprocess
import sys
import tempfile
import time
import traceback

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DLL = os.path.join(ROOT, "dll")
sys.path.insert(0, os.path.join(ROOT, "tests"))
os.environ.setdefault("OLYMPIAN_LANG", "es")
import winstubs  # noqa: E402,F401


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


py = load(os.path.join(ROOT, "mod", "OlympianPlanetCoords.py"), "OlympianPlanetCoords_ref")
ov = load(os.path.join(ROOT, "overlay", "olympian_overlay.py"), "olympian_overlay_ref")

BUILD = tempfile.mkdtemp(prefix="opc_dll_")
SRCS = ["tests/core_harness.cpp", "src/core.cpp", "src/http_server.cpp"]
FLAGS = ["-O1", "-std=c++17", "-Wall", "-Wextra", "-Isrc", "-Ithird_party"]


def build_native():
    exe = os.path.join(BUILD, "core_harness")
    cmd = ["g++", *FLAGS, *[os.path.join(DLL, s) for s in SRCS], "-o", exe, "-pthread"]
    subprocess.run(cmd, cwd=DLL, check=True)
    return exe


class Harness:
    def __init__(self, args):
        self.p = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, encoding="utf-8")

    def ask(self, obj):
        self.p.stdin.write(json.dumps(obj) + "\n")
        self.p.stdin.flush()
        return json.loads(self.p.stdout.readline())

    def close(self):
        self.p.stdin.close()
        self.p.wait(timeout=5)


def close(a, b, tol=1e-9):
    return abs(a - b) <= tol * max(1.0, abs(a), abs(b))


R = 3000.0
CENTER = (100.0, -50.0, 20.0)
KEY = "G0:10:-3:7:42:P1"


def pos_at(lat, lon, radius=R, conv=0, inv=False):
    u = py.latlon_to_unit(lat, lon, conv, inv)
    return [CENTER[0] + u[0] * radius, CENTER[1] + u[1] * radius, CENTER[2] + u[2] * radius]


def frame(lat, lon, t, location=3, radius=R, **kw):
    f = {"pos": pos_at(lat, lon, radius), "center": list(CENTER), "t": t, "location": location}
    f.update(kw)
    return f


TESTS = []


def test(fn):
    TESTS.append(fn)
    return fn


@test
def test_geo_igual_a_python(exe):
    h = Harness([exe, "geo"])
    rnd = random.Random(7)
    for _ in range(400):
        rel = [rnd.uniform(-5000, 5000) for _ in range(3)]
        conv, inv = rnd.randrange(4), rnd.random() < 0.5
        lat, lon = h.ask({"fn": "v2ll", "rel": rel, "conv": conv, "inv": inv})
        plat, plon = py.vector_to_latlon(tuple(rel), py.LonConvention(conv), inv)
        assert close(lat, plat) and close(lon, plon), (rel, conv, inv, lat, lon, plat, plon)
        la, lo = rnd.uniform(-90, 90), rnd.uniform(-180, 180)
        u = h.ask({"fn": "ll2u", "lat": la, "lon": lo, "conv": conv, "inv": inv})
        pu = py.latlon_to_unit(la, lo, py.LonConvention(conv), inv)
        assert all(close(a, b) for a, b in zip(u, pu)), (u, pu)
        a, b = [rnd.uniform(-90, 90), rnd.uniform(-180, 180)], [rnd.uniform(-90, 90), rnd.uniform(-180, 180)]
        r = rnd.uniform(100, 500000)
        assert close(h.ask({"fn": "dist", "a": a, "b": b, "r": r}), py.surface_distance(*a, *b, r))
        brg = h.ask({"fn": "brg", "a": a, "b": b})
        assert close(brg, py.initial_bearing(*a, *b), 1e-9), (brg, py.initial_bearing(*a, *b))
        for lang in ("es", "en"):
            assert h.ask({"fn": "card", "b": brg, "lang": lang}) == py.cardinal(brg, lang)
        vecs = {k: [rnd.uniform(-10, 10) for _ in range(3)] for k in ("o", "t", "r", "u", "at")}
        flip = rnd.random() < 0.5
        y, p = h.ask({"fn": "reldir", **vecs, "flip": flip})
        py_, pp = py.relative_direction(vecs["o"], vecs["t"], vecs["r"], vecs["u"], vecs["at"], flip)
        assert close(y, py_) and close(p, pp)
    for b in (0, 22.4999, 22.5, 67.5, 359.99, 360, -10, 720.5):
        for lang in ("es", "en"):
            assert h.ask({"fn": "card", "b": b, "lang": lang}) == py.cardinal(b, lang), (b, lang)
    for v, d in ((2.675, 2), (0.125, 2), (12.345, 2), (-3.14159, 1), (1234.55, 1), (2.5, 0), (3.5, 0)):
        assert close(h.ask({"fn": "round", "v": v, "d": d}), round(v, d)), (v, d)
    h.close()


@test
def test_coords_igual_que_python(exe):
    base = tempfile.mkdtemp()
    path = os.path.join(base, "datos.json")
    h = Harness([exe, "core", path])
    s = h.ask({"none": True, "t": 0})
    assert s["on_planet"] is False and s["reason"] == "esperando al juego" and s["lat"] is None, s
    assert s["version"].startswith("0.1.0-dll") and s["lang"] == "es" and s["hotkey"] == "f9", s
    assert s["hotkey_saved"] == "f9" and s["input_request_id"] == 0, s
    assert s["history"] == [] and s["last_result"] == {"ok": None, "message": "", "seq": 0}
    for lat, lon in ((12.3456, -45.6789), (-80.1, 179.9), (0.0, 0.0), (45.0, -179.99), (89.0, 10.0)):
        s = h.ask(frame(lat, lon, 1))
        plat, plon = py.vector_to_latlon(tuple(a - b for a, b in zip(pos_at(lat, lon), CENTER)), py.LonConvention(0), False)
        assert s["on_planet"] is True and s["in_orbit"] is False and s["reason"] == "", s
        assert s["lat"] == round(plat, 2) and s["lon"] == round(plon, 2), (s, plat, plon)
        assert s["planet_key"] == KEY and s["destination"] is None
    with open(path, encoding="utf-8") as f:
        saved = json.load(f)
    assert abs(saved["radii"][KEY] - R) < 0.1, saved["radii"]
    # fuera del planeta (espacio) cerca: orbita con el radio guardado; lejos: no estas en un planeta
    s = h.ask(frame(10, 20, 2, location=1, radius=2.5 * R))
    assert s["in_orbit"] is True and s["on_planet"] is False and s["lat"] == 10.0 and s["lon"] == 20.0, s
    s = h.ask(frame(10, 20, 3, location=1, radius=3.5 * R))
    assert s["in_orbit"] is False and s["reason"] == "no estas en un planeta", s
    s = h.ask(frame(10, 20, 3, location=2))  # estacion espacial
    assert s["reason"] == "no estas en un planeta", s
    h.close()


@test
def test_destino_y_llegada_igual_que_python(exe):
    base = tempfile.mkdtemp()
    path = os.path.join(base, "datos.json")
    data = json.loads(json.dumps(py.DEFAULT_DATA))
    data["destinations"][KEY] = {"lat": 15.0, "lon": -40.0, "name": "Torre"}
    data["history"] = [{"lat": 15.0, "lon": -40.0, "name": "Torre"}]
    data["settings"]["arrow_flip"] = True
    py.save_data(path, data)  # escrito por la version Python
    h = Harness([exe, "core", path])
    lat, lon = 12.0, -45.0
    at = [0.3, 0.1, 0.95]
    s = h.ask(frame(lat, lon, 0.0, at=at, right=[0.95, 0, -0.3], up=[0, 1, 0]))
    d = s["destination"]
    dist = py.surface_distance(lat, lon, 15.0, -40.0, R)
    brg = py.initial_bearing(lat, lon, 15.0, -40.0)
    u = py.latlon_to_unit(15.0, -40.0, 0, False)
    target = tuple(c + k * R for c, k in zip(CENTER, u))
    yaw, pitch = py.relative_direction(tuple(pos_at(lat, lon)), target, (0.95, 0, -0.3), (0, 1, 0), tuple(at), True)
    assert d["name"] == "Torre" and d["lat"] == 15.0 and d["lon"] == -40.0, d
    assert abs(d["distance_m"] - round(dist, 1)) <= 0.1, (d, dist)
    assert d["bearing"] == round(brg) and d["cardinal"] == py.cardinal(brg, "es"), (d, brg)
    assert abs(d["yaw"] - round(yaw, 1)) <= 0.1 and abs(d["pitch"] - round(pitch, 1)) <= 0.1, (d, yaw, pitch)
    assert s["history"] == data["history"]
    # a menos de 50 m pero recien llegado (no estable): todavia no cuenta
    near = (15.0 + 0.0003, -40.0)
    s = h.ask(frame(*near, 0.5))
    assert s["destination"] is not None, s
    s = h.ask(frame(*near, 1.6))
    assert s["destination"] is None and s["last_result"]["message"] == "Llegaste a Torre", s
    assert s["last_result"]["ok"] is True and s["last_result"]["seq"] == 1
    loaded = py.load_data(path)  # la version Python lee lo que escribio el DLL
    assert KEY not in loaded["destinations"] and loaded["history"] == data["history"]
    assert loaded["settings"]["arrow_flip"] is True and abs(loaded["radii"][KEY] - R) < 0.1
    h.close()


@test
def test_idioma_y_peticiones(exe):
    base = tempfile.mkdtemp()
    path = os.path.join(base, "datos.json")
    data = json.loads(json.dumps(py.DEFAULT_DATA))
    data["destinations"][KEY] = {"lat": 12.0, "lon": -30.0, "name": ""}
    py.save_data(path, data)
    h = Harness([exe, "core", path])
    assert h.ask({"put": "lang", "query": {"code": "fr"}}) == {"saved": False, "message": "idioma no valido"}
    assert h.ask({"put": "lang", "query": {"code": "EN"}}) == {"saved": True, "lang": "en", "gui_lang": "en"}
    s = h.ask(frame(12.0, -45.0, 0))
    assert s["lang"] == "en" and s["destination"]["cardinal"] == "E", s
    s = h.ask(frame(12.0, -45.0, 0, location=2))
    assert s["reason"] == "you're not on a planet", s
    s = h.ask(frame(12.0, -30.0002, 0.2))
    s = h.ask(frame(12.0, -30.0002, 1.5))
    assert s["last_result"]["message"] == "You reached +12.00, -30.00", s
    with open(path, encoding="utf-8") as f:
        assert json.load(f)["settings"]["lang"] == "en"
    h.close()


@test
def test_datos_danados_y_utf8(exe):
    base = tempfile.mkdtemp()
    path = os.path.join(base, "datos.json")
    with open(path, "w", encoding="utf-8") as f:
        f.write("{esto no es json")
    h = Harness([exe, "core", path])
    s = h.ask(frame(1, 2, 0))
    assert s["on_planet"] is True
    assert os.path.exists(path + ".bak"), "el archivo danado se guarda como .bak"
    h.close()
    data = json.loads(json.dumps(py.DEFAULT_DATA))
    data["destinations"][KEY] = {"lat": 5.0, "lon": 5.0, "name": "Montaña Ñandú 星"}
    py.save_data(path, data)
    h = Harness([exe, "core", path])
    s = h.ask(frame(1, 2, 0))
    assert s["destination"]["name"] == "Montaña Ñandú 星", s
    h.close()


def wait_port(port, timeout=10.0):
    import socket

    t0 = time.time()
    while time.time() - t0 < timeout:
        try:
            socket.create_connection(("127.0.0.1", port), timeout=0.3).close()
            return True
        except OSError:
            time.sleep(0.1)
    return False


@test
def test_destinos_historial_tecla_y_marcador(exe):
    """PUT /marker, /clear y /hotkey, el historial y el marcador del HUD, contra la version Python."""
    base = tempfile.mkdtemp()
    path = os.path.join(base, "datos.json")
    h = Harness([exe, "core", path])
    # sin juego: se acepta la peticion, pero el resultado dice por que no se puso
    r = h.ask({"put": "marker", "query": {"lat": "10", "lon": "20", "name": "x"}})
    assert r == {"queued": True}, r
    s = h.ask({"none": True, "t": 0})
    assert s["last_result"]["ok"] is False and s["last_result"]["message"] == "esperando al juego", s
    # fuera de rango / no es numero (Python: out_of_range)
    for q in ({"lat": "91", "lon": "0"}, {"lat": "0", "lon": "-180.5"}, {"lat": "abc", "lon": "1"}, {"lon": "1"}):
        r = h.ask({"put": "marker", "query": q})
        assert r == {"queued": False, "message": py.tr("out_of_range", "es")}, (q, r)
    s = h.ask(frame(12.0, -45.0, 0.0))
    assert h.ask({"marker": True})["valid"] is False
    # poner destino: mismo mensaje, mismo datos.json y mismo historial que Python
    name = "  Torre Ñandú del valle con un nombre largo largo largo largo largo largo  "
    r = h.ask({"put": "marker", "query": {"lat": "15.006", "lon": "-40,004", "name": name}})
    assert r == {"queued": True}, r
    pname = name.strip()[:60]
    s = h.ask(frame(12.0, -45.0, 0.5))
    exp_msg = py.tr("dest_set", "es", coords=f"{15.006:+.2f}, {-40.004:+.2f}" + f" {pname}")
    assert s["last_result"] == {"ok": True, "message": exp_msg, "seq": 2}, s["last_result"]
    d = s["destination"]
    assert d["lat"] == round(15.006, 2) and d["lon"] == round(-40.004, 2) and d["name"] == pname, d
    exp_hist = py.push_history([], 15.006, -40.004, pname)
    assert s["history"] == exp_hist, (s["history"], exp_hist)
    saved = py.load_data(path)
    assert saved["destinations"][KEY] == {"lat": 15.01, "lon": -40.0, "name": pname}, saved["destinations"]
    # el marcador aparece 1 s despues de llegar al planeta (como _sync_marker con ctx["stable"])
    assert h.ask({"marker": True})["valid"] is False
    h.ask(frame(12.0, -45.0, 1.2))
    m = h.ask({"marker": True})
    u = py.latlon_to_unit(15.01, -40.0, 0, False)
    exp = [c + k * R for c, k in zip(CENTER, u)]  # radio = distancia al centro medida (3000)
    assert m["valid"] is True and all(abs(a - b) < 0.01 for a, b in zip(m["local"], exp)), (m, exp)
    assert m["offset"] == [0.0, 0.0, 0.0], m
    assert m["label"] == pname.encode("utf-8")[:63].decode("utf-8", "ignore"), m["label"]
    assert len(m["label"].encode("utf-8")) <= 63
    # sin nombre: "Destino +lat,+lon" (como Python)
    h.ask({"put": "marker", "query": {"lat": "-3.5", "lon": "7"}})
    h.ask(frame(12.0, -45.0, 1.3))
    assert h.ask({"marker": True})["label"] == "Destino -3.50,+7.00"
    # historial: el repetido sube al principio, maximo 10
    hist = s["history"]
    for i, (la, lo) in enumerate([(1, 1), (2, 2), (15.01, -40.0), *[(k, k) for k in range(3, 12)]]):
        h.ask({"put": "marker", "query": {"lat": str(la), "lon": str(lo), "name": f"n{i}"}})
        hist = py.push_history(hist if i else py.push_history(exp_hist, -3.5, 7, ""), la, lo, f"n{i}")
    s = h.ask(frame(12.0, -45.0, 1.4))
    assert s["history"] == hist and len(hist) == 10, (s["history"], hist)
    # quitar
    assert h.ask({"put": "clear", "query": {}}) == {"queued": True}
    s = h.ask(frame(12.0, -45.0, 1.5))
    assert s["destination"] is None and s["last_result"]["message"] == py.tr("dest_cleared", "es"), s
    assert h.ask({"marker": True})["valid"] is False
    h.ask({"put": "clear", "query": {}})
    s = h.ask(frame(12.0, -45.0, 1.6))
    assert s["last_result"] == {"ok": False, "message": py.tr("no_dest_here", "es"), "seq": s["last_result"]["seq"]}
    # al cambiar de planeta el marcador espera 1 s otra vez
    h.ask({"put": "marker", "query": {"lat": "1", "lon": "1"}})
    h.ask(frame(12.0, -45.0, 3.0))
    assert h.ask({"marker": True})["valid"] is True
    h.ask(frame(12.0, -45.0, 3.1, planet_index=2))
    assert h.ask({"marker": True})["valid"] is False  # otro planeta, sin destino
    h.ask(frame(12.0, -45.0, 3.2))  # de vuelta al planeta 1: recien llegado
    assert h.ask({"marker": True})["valid"] is False
    h.ask(frame(12.0, -45.0, 4.3))
    assert h.ask({"marker": True})["valid"] is True
    h.ask({"none": True, "t": 4.4})  # pantalla de carga / menu: sin marcador
    assert h.ask({"marker": True})["valid"] is False
    # tecla: F9 por defecto (VK 0x78); se cambia al momento (sin Reload Mod)
    assert h.ask({"marker": True})["vk"] == 0x78
    assert h.ask({"put": "hotkey", "query": {"key": "F12"}}) == {"saved": False, "message": py.tr("bad_hotkey", "es")}
    r = h.ask({"put": "hotkey", "query": {"key": " F7 "}})
    assert r == {"saved": True, "hotkey": "f7", "hotkey_saved": "f7", "pending_reload": False}, r
    assert h.ask({"marker": True})["vk"] == 0x76
    assert py.configured_hotkey(path) == "f7"  # la version Python lee la misma tecla
    # pulsaciones: cuenta al bajar la tecla, con el juego en primer plano; mantenerla no repite
    s = h.ask({"key": True, "focused": True})
    assert s["input_request_id"] == 1 and s["hotkey"] == "f7" and s["hotkey_saved"] == "f7", s
    assert h.ask({"key": True, "focused": True})["input_request_id"] == 1
    assert h.ask({"key": False})["input_request_id"] == 1
    assert h.ask({"key": True, "focused": False})["input_request_id"] == 1  # otra ventana tenia el foco
    h.ask({"key": False})
    assert h.ask({"key": True, "focused": True})["input_request_id"] == 2
    r = h.ask({"put": "hotkey", "query": {"key": "ninguna"}})
    assert r["saved"] is True and h.ask({"marker": True})["vk"] == 0
    h.close()
    # al reiniciar, todo sigue ahi (y en ingles los textos cambian)
    h = Harness([exe, "core", path])
    h.ask({"put": "lang", "query": {"code": "en"}})
    s = h.ask(frame(12.0, -45.0, 0))
    assert s["hotkey"] == "ninguna" and s["destination"]["lat"] == 1.0 and len(s["history"]) == 10, s
    h.ask(frame(12.0, -45.0, 1.1))
    h.ask({"put": "marker", "query": {"lat": "2", "lon": "3"}})
    h.ask(frame(12.0, -45.0, 1.2))
    assert h.ask({"marker": True})["label"] == "Destination +2.00,+3.00"
    h.close()


def check_server_with_overlay_client(port):
    client = ov.ModClient("127.0.0.1", port)
    s = client.get_coords()
    assert s["on_planet"] is True and s["planet_key"] == KEY and isinstance(s["lat"], float), s
    assert client.set_lang("en") == {"saved": True, "lang": "en", "gui_lang": "en"}
    r = client.place_marker(10.0, 20.0, "Tower ñ")
    assert r == {"queued": True}, r
    r = client.set_hotkey("f6")
    assert r == {"saved": True, "hotkey": "f6", "hotkey_saved": "f6", "pending_reload": False}, r
    import urllib.error
    import urllib.request

    try:
        urllib.request.urlopen(f"http://127.0.0.1:{port}/nada", timeout=1)
        raise AssertionError("debe dar 404")
    except urllib.error.HTTPError as e:
        assert e.code == 404
    d = json.loads(urllib.request.urlopen(f"http://127.0.0.1:{port}/OlympianPlanetCoords/debug", timeout=1).read())
    assert d["credits"]["license"] == "GPL-3.0" and d["context"]["planet_key"] == KEY, d.keys()
    s2 = client.get_coords()
    assert s2["lang"] == "en" and s2["hotkey"] == "f6", s2
    assert s2["last_result"]["message"] == "Destination: +10.00, +20.00 Tower ñ", s2["last_result"]
    assert s2["history"][0] == {"lat": 10.0, "lon": 20.0, "name": "Tower ñ"}, s2["history"]
    return s2


@test
def test_servidor_http_con_el_cliente_del_overlay(exe):
    base = tempfile.mkdtemp()
    port = 5077
    p = subprocess.Popen([exe, "serve", os.path.join(base, "datos.json"), str(port)], stdout=subprocess.PIPE, text=True)
    try:
        assert wait_port(port), "el servidor no abrio"
        time.sleep(0.2)
        check_server_with_overlay_client(port)
        # muchas peticiones seguidas, como el overlay (cada 60 ms)
        client = ov.ModClient("127.0.0.1", port)
        for _ in range(200):
            client.get_coords()
    finally:
        p.terminate()
        p.wait(timeout=5)


@test
def test_windows_wine_fakegame(exe):
    if not (shutil.which("wine") and shutil.which("x86_64-w64-mingw32-g++")):
        print("   (sin wine/mingw: se omite)")
        return
    wexe = os.path.join(BUILD, "core_harness.exe")
    srcs = SRCS + ["src/game_reader.cpp"]
    subprocess.run(["x86_64-w64-mingw32-g++", *FLAGS, *[os.path.join(DLL, s) for s in srcs], "-o", wexe,
                    "-static", "-lws2_32"], cwd=DLL, check=True)
    base = tempfile.mkdtemp()
    port = 5078
    env = dict(os.environ, WINEDEBUG="-all")
    p = subprocess.Popen(["wine", wexe, "fakegame", os.path.join(base, "datos.json"), str(port)],
                         stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, env=env)
    try:
        first = p.stdout.readline().strip()
        assert first == "empty_app_read=0", first  # sin mpData: no lee nada (esperando al juego)
        assert wait_port(port, 30), "el servidor (wine) no abrio"
        time.sleep(0.5)
        s = check_server_with_overlay_client(port)
        assert -14.5 < s["lat"] < 14.5 and -46 < s["lon"] < 0, s  # el jugador falso camina desde (12, -45)
    finally:
        p.terminate()
        p.wait(timeout=10)


@test
def test_dll_completo_bajo_wine_con_nms_falso(exe):
    """El winmm.dll real cargado por un NMS.exe falso (version 185034, las 9 funciones en .text, un
    cTkFSM señuelo y la aplicacion verdadera; Update recibe basura en rcx, como en el juego real)."""
    if not (shutil.which("wine") and shutil.which("x86_64-w64-mingw32-g++")):
        print("   (sin wine/mingw: se omite)")
        return
    subprocess.run(["sh", "build.sh"], cwd=DLL, check=True, stdout=subprocess.DEVNULL)
    fake = os.path.join(DLL, "tests", "fake_nms")
    subprocess.run([sys.executable, os.path.join(fake, "gen_fake.py")], check=True, stdout=subprocess.DEVNULL)
    game = tempfile.mkdtemp(prefix="fakegame_")
    res = os.path.join(BUILD, "fake_res.o")
    subprocess.run(["x86_64-w64-mingw32-windres", os.path.join(fake, "fake_nms.rc"), "-O", "coff", "-o", res], check=True)
    subprocess.run(["x86_64-w64-mingw32-g++", "-O1", "-std=c++17", os.path.join(fake, "fake_nms.cpp"),
                    os.path.join(fake, "sigs.S"), res, "-o", os.path.join(game, "NMS.exe"), "-lwinmm"], check=True)
    shutil.copy(os.path.join(DLL, "build", "winmm.dll"), game)
    env = dict(os.environ, WINEDEBUG="-all", WINEDLLOVERRIDES="winmm=n,b")
    p = subprocess.Popen(["wine", "NMS.exe", "12"], cwd=game, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                         text=True, env=env)
    try:
        assert wait_port(5000, 30), "el DLL no abrio el puerto 5000"
        deadline = time.time() + 15
        s = {}
        while time.time() < deadline:
            s = ov.ModClient("127.0.0.1", 5000).get_coords()
            if s.get("on_planet"):
                break
            time.sleep(0.3)
        assert s.get("on_planet") is True and s["planet_key"] == KEY, s
        import urllib.request

        d = json.loads(urllib.request.urlopen("http://127.0.0.1:5000/OlympianPlanetCoords/debug", timeout=2).read())
        assert d["dll"]["functions_found"] == d["dll"]["functions_total"] == 9, d["dll"]
        assert d["dll"]["fsm_candidates"] == 2 and d["dll"]["frames_with_game"] > 0, d["dll"]
        # destino lejos del jugador simulado (no llega antes de terminar)
        client = ov.ModClient("127.0.0.1", 5000)
        assert client.place_marker(-20.0, 100.0, "Torre Ñ") == {"queued": True}
        time.sleep(2.5)
        d = json.loads(urllib.request.urlopen("http://127.0.0.1:5000/OlympianPlanetCoords/debug", timeout=2).read())
        assert d["active_marker"] is True and d["marker_frames_drawn"] > 0, d
        assert d["marker_frames_skipped"] > 0 and d["dll"]["bbm_frames"] > 0, d  # el BBM sin datos se salta
        out, _ = p.communicate(timeout=30)
        assert "timeGetTime=" in out and "frames=" in out, out  # el winmm real sigue funcionando
        log = open(os.path.join(game, "Olympian", "olympian.log"), encoding="utf-8").read()
        assert "cGcApplication reconocido (candidato 2 de 2)" in log, log
        assert "Hook cGcBaseBuildingManager::Update (marcador del HUD): MH_OK" in log, log
        res = dict(kv.split("=", 1) for line in out.splitlines() for kv in line.split(" ") if "=" in kv)
        assert int(res["hud_calls"]) > 0 and res["hud_bad_this"] == "0", out
        assert int(res["bbm_body"]) >= 2 * int(res["hud_calls"]), out  # el juego sigue haciendo lo suyo
        assert res["type"] == "0x35" and res["colour"] == "0" and res["hud_right"] == "1.0,0.0,0.0", out
        assert out.split("hud_name=", 1)[1].splitlines()[0] == "Torre Ñ", out
        u = py.latlon_to_unit(-20.0, 100.0, 0, False)
        exp = [c + k * R for c, k in zip(CENTER, u)]
        got = [float(x) for x in res["hud_local"].split(",")]
        assert all(abs(a - b) < 0.05 for a, b in zip(got, exp)), (got, exp)
        assert res["hud_offset"] == "0.000,0.000,0.000", out
    finally:
        if p.poll() is None:
            p.terminate()
            p.wait(timeout=10)


if __name__ == "__main__":
    exe = build_native()
    fails = 0
    for t in TESTS:
        t0 = time.time()
        try:
            t(exe)
            print(f"OK   {t.__name__} ({time.time() - t0:.1f}s)")
        except Exception:
            fails += 1
            print(f"FAIL {t.__name__}")
            traceback.print_exc()
    print(f"FALLOS: {fails}")
    sys.exit(1 if fails else 0)
