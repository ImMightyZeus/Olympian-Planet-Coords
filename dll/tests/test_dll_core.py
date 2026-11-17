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
    assert s["version"].startswith("0.1.0-dll") and s["lang"] == "es" and s["hotkey"] == "ninguna", s
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
    for route, q in (("marker", {"lat": "1", "lon": "2", "name": "x"}), ("clear", {}), ("hotkey", {"key": "f7"})):
        r = h.ask({"put": route, "query": q})
        assert r["queued"] is False and r["saved"] is False and "next DLL version" in r["message"], r
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


def check_server_with_overlay_client(port, expect_text):
    client = ov.ModClient("127.0.0.1", port)
    s = client.get_coords()
    assert s["on_planet"] is True and s["planet_key"] == KEY and isinstance(s["lat"], float), s
    assert client.set_lang("en") == {"saved": True, "lang": "en", "gui_lang": "en"}
    r = client.place_marker(10.0, 20.0, "Tower ñ")
    assert r["queued"] is False and expect_text in r["message"], r
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
    assert s2["lang"] == "en"
    return s2


@test
def test_servidor_http_con_el_cliente_del_overlay(exe):
    base = tempfile.mkdtemp()
    port = 5077
    p = subprocess.Popen([exe, "serve", os.path.join(base, "datos.json"), str(port)], stdout=subprocess.PIPE, text=True)
    try:
        assert wait_port(port), "el servidor no abrio"
        time.sleep(0.2)
        check_server_with_overlay_client(port, "next DLL version")
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
        s = check_server_with_overlay_client(port, "next DLL version")
        assert -14.5 < s["lat"] < 14.5 and -46 < s["lon"] < 0, s  # el jugador falso camina desde (12, -45)
    finally:
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
