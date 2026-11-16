"""Pruebas del overlay v0.0.3 sin servidor: tecla configurable, orbita, destino, resultados del juego, historial,
estado offline, fuentes, Acerca de, tamano, flecha 3D / esfera y errores persistentes."""
import importlib.util
import json
import os
import sys
import tempfile
import time

spec = importlib.util.spec_from_file_location(
    "olympian_overlay", os.path.join(os.path.dirname(__file__), "..", "overlay", "olympian_overlay.py"))
ov = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ov)


class FakeClient:
    def get_coords(self):
        raise OSError("offline")


# ---- geometria pura de la flecha
g0 = ov.arrow_geometry(0, 0, 0)
assert len(g0["faces"]) == 6 and g0["sphere"] == [] and g0["ring_r"] == ov.RING_R


def tip(geo):
    """Vertice mas alejado del centro = punta de la flecha."""
    pts = [p for f in geo["faces"] for p in f["points"]]
    return max(pts, key=lambda p: p[0] ** 2 + p[1] ** 2)


assert tip(ov.arrow_geometry(0, 0, 0))[1] < -10, "de frente: la punta apunta hacia arriba"
assert tip(ov.arrow_geometry(90, 0, 0))[0] > 10, "a la derecha: la punta apunta a la derecha"
assert tip(ov.arrow_geometry(180, 0, 0))[1] > 10, "detras: la punta apunta hacia abajo"


def length(geo):
    pts = [p for f in geo["faces"] for p in f["points"]]
    return max(p[1] for p in pts) - min(p[1] for p in pts)


assert length(ov.arrow_geometry(0, 40, 0)) > length(ov.arrow_geometry(0, -40, 0)), "arriba se alarga"
gi = ov.arrow_geometry(0, 0, 1)
assert gi["faces"] == [] and abs(gi["ring_r"] - ov.RING_R * 0.4) < 1e-9, "reposo: circulo al 40%"
outer = max(abs(s["cx"]) + s["r"] for s in gi["sphere"])
assert outer < gi["ring_r"] - 1, "esfera y circulo concentricos sin tocarse"
assert abs(ov.approach_angle(170, -170, 1.0) - 190) < 1e-9, "gira por el camino corto"

# ---- flecha suavizada con Pillow (lo mismo que se ve en Windows)
from PIL import Image  # noqa: E402

for geo, size in ((ov.arrow_geometry(35, 15, 0), 44), (ov.arrow_geometry(0, 0, 1), 44), (g0, 120)):
    img = ov.render_arrow_image(geo, size)
    assert img.mode == "RGBA" and img.size == (size, size)
    alphas = set(img.getchannel("A").tobytes())
    assert 0 in alphas and 255 in alphas, "fondo transparente y figura opaca"
    partial = [a for a in img.getchannel("A").tobytes() if 0 < a < 255]
    assert len(partial) > size, "bordes suaves: debe haber pixeles semitransparentes"
    # premultiplicado BGRA: ningun canal de color supera al alfa, y el orden es B,G,R,A
    raw = ov.premultiplied_bgra(img)
    assert len(raw) == size * size * 4
    for i in range(0, len(raw), 4):
        b_, g_, r_, a_ = raw[i:i + 4]
        assert b_ <= a_ and g_ <= a_ and r_ <= a_
    corner = raw[0:4]
    assert corner == b"\x00\x00\x00\x00", corner
# un pixel opaco del circulo conserva su color (#A8740F) en orden BGRA
ring_img = ov.render_arrow_image(ov.arrow_geometry(0, 0, 1), 200)
opaque = [(x, y) for x in range(200) for y in range(200) if ring_img.getpixel((x, y))[3] == 255]
px = ring_img.getpixel(opaque[len(opaque) // 2])
raw = ov.premultiplied_bgra(ring_img)
i = (opaque[len(opaque) // 2][1] * 200 + opaque[len(opaque) // 2][0]) * 4
assert tuple(raw[i:i + 4]) == (px[2], px[1], px[0], 255)

# ---- ventana con alfa por pixel: se simula la API de Windows y se comprueba cada llamada con
#      los argtypes declarados (ctypes valida tipos y cantidad de argumentos)
import ctypes  # noqa: E402


class FakeFn:
    def __init__(self, name, log, ret):
        self.name, self.log, self.ret = name, log, ret
        self.argtypes = None
        self.restype = None

    def __call__(self, *args):
        assert self.argtypes is not None, f"{self.name} sin argtypes"
        assert len(args) == len(self.argtypes), (self.name, args)
        for t, a in zip(self.argtypes, args):
            t.from_param(a)
        self.log.append((self.name, args))
        return self.ret(*args) if callable(self.ret) else self.ret


class FakeDLL:
    BITS = ctypes.create_string_buffer(4 * 400 * 400)

    def __init__(self, name, use_last_error=False):
        self.log = WIN_LOG

        def dib(hdc, bmi, usage, ppbits, sec, off):
            ppbits._obj.value = ctypes.addressof(FakeDLL.BITS)
            return 0x4444

        rets = {"GetDC": 0x1111, "ReleaseDC": 1, "GetWindowLongPtrW": 0x100, "SetWindowLongPtrW": 0x100,
                "UpdateLayeredWindow": 1, "CreateCompatibleDC": 0x2222, "DeleteDC": 1,
                "CreateDIBSection": dib, "SelectObject": 0x3333, "DeleteObject": 1}
        for k, v in rets.items():
            setattr(self, k, FakeFn(k, self.log, v))


WIN_LOG = []
_real_windll = getattr(ctypes, "WinDLL", None)
ctypes.WinDLL = FakeDLL
ctypes.get_last_error = lambda: 0
_root = ov.tk.Tk()
lw = ov.LayeredArrowWindow(_root)
names = [n for n, _ in WIN_LOG]
assert names == ["GetWindowLongPtrW", "SetWindowLongPtrW"], names
assert WIN_LOG[1][1][2] == 0x100 | 0x00080000 | 0x00000080, "WS_EX_LAYERED | WS_EX_TOOLWINDOW"
img = ov.render_arrow_image(ov.arrow_geometry(20, 5, 0), 60)
lw.show(ov.premultiplied_bgra(img), 60, 60, 300, 40)
names = [n for n, _ in WIN_LOG[2:]]
assert names == ["GetDC", "CreateCompatibleDC", "CreateDIBSection", "SelectObject", "UpdateLayeredWindow",
                 "SelectObject", "DeleteObject", "DeleteDC", "ReleaseDC"], names
assert FakeDLL.BITS.raw[:60 * 60 * 4] == ov.premultiplied_bgra(img), "los pixeles llegan al DIB"
ulw = dict(WIN_LOG)["UpdateLayeredWindow"]
assert ulw[0] == lw.hwnd and ulw[-1] == 2, "ULW_ALPHA"
try:
    lw.show(b"1234", 60, 60, 0, 0)
    raise AssertionError("debe rechazar un tamano inconsistente")
except ValueError:
    pass
lw.destroy()
_root.destroy()
if _real_windll is None:
    del ctypes.WinDLL
else:
    ctypes.WinDLL = _real_windll

# ---- ventana
tmp = tempfile.mkdtemp()
sfile = os.path.join(tmp, "overlay.json")
o = ov.Overlay(FakeClient(), 0, 0, poll_ms=50, settings_file=sfile)
assert o.fonts_ok, "las fuentes DSEG7 Classic y Share Tech Mono deben estar disponibles"
assert o.settings["size"] == "normal" and o.fonts["num"].cget("size") == 8
res0 = {"ok": None, "message": "", "seq": 0}
base = {"ok": True, "on_planet": True, "in_orbit": False, "lat": 1.0, "lon": -2.5, "destination": None,
        "history": [], "last_result": res0, "input_request_id": 0}
o._on_coords(dict(base))
assert o._layout == ["now"] and not o.editing
assert o.now_row.text() == " +1.00,   -2.50", repr(o.now_row.text())


def run_frames(n, dt=0.034):
    for _ in range(n):
        o._last_frame = time.monotonic() - dt
        o._render_loop()
        o.root.update_idletasks()


# sin destino: esfera en reposo
run_frames(3)
assert o.morph == 1.0
items = o.canvas.find_all()
assert len([i for i in items if o.canvas.type(i) == "polygon"]) == 0
assert len([i for i in items if o.canvas.type(i) == "oval"]) == 1 + len(ov.SPHERE_LAYERS)
# con destino: la flecha aparece en ~0.5 s y apunta a su giro
dest = {"lat": 14.5, "lon": -41.0, "name": "Torre", "distance_m": 1834.0, "bearing": 57, "cardinal": "NE",
        "yaw": 90.0, "pitch": 10.0}
o._on_coords(dict(base, destination=dest))
assert o._layout == ["now", "dest", "dest_info"]
assert o.dest_info.cget("text") == "a 1.8 km · rumbo 57° NE"
run_frames(30)
assert o.morph == 0.0 and abs(o.arrow_yaw - 90) < 2 and abs(o.arrow_pitch - 10) < 1, (o.morph, o.arrow_yaw)
assert len([i for i in o.canvas.find_all() if o.canvas.type(i) == "polygon"]) == 6
# llegada: vuelve a esfera
o._on_coords(dict(base, last_result={"ok": True, "message": "Llegaste a Torre", "seq": 1}))
run_frames(20)
assert o.morph == 1.0
assert o.status.cget("text") == "Llegaste a Torre" and o.status.cget("fg") == ov.OK
# error del juego: queda hasta la siguiente accion
o._on_coords(dict(base, last_result={"ok": False, "message": "el juego rechazo el marcador", "seq": 2}))
o.status_until = 1  # aunque pase el tiempo...
o._tick()
assert o.status.cget("text") == "el juego rechazo el marcador" and o.status.cget("fg") == ov.ERR
o.start_edit()  # ...se limpia con la siguiente accion
assert o.status.cget("text") == ov.HINT_EDIT
o.cancel_edit()
# tamano grande y persistencia
o.set_size("grande")
assert o.fonts["num"].cget("size") == 15 and json.load(open(sfile))["size"] == "grande"
o2_settings = ov.load_settings(sfile)
assert o2_settings["size"] == "grande"
o.set_size("normal")
# orbita
o._on_coords(dict(base, in_orbit=True, on_planet=False, input_request_id=0))
assert o.orbit_tag.cget("text") == "ÓRBITA"
# tecla dentro del juego (F9 por defecto)
assert o.arrow_mode == "canvas"  # en Linux no hay ventana con alfa: se usa el Canvas
o._on_coords(dict(base, input_request_id=1, hotkey="f9", hotkey_saved="f9"))
assert o.editing and o._layout == ["now", "entry"], "la tecla debe abrir el campo"
assert o.hotkey_var.get() == "f9"
o.cancel_edit()
assert o.status.cget("text") == "clic para ir a coordenadas · F9", o.status.cget("text")
o._results.put(("hotkey", {"saved": True, "hotkey": "f9", "hotkey_saved": "f7", "pending_reload": True}))
o._drain()
assert "F7 guardada" in o.status.cget("text") and "Reload Mod" in o.status.cget("text")
o._results.put(("hotkey", {"saved": False, "message": "tecla no valida"}))
o._drain()
assert o.status.cget("text") == "tecla no valida" and o.status_sticky
o.start_edit()
# historial
o.history = [{"lat": 14.5, "lon": -41.0, "name": "Torre"}, {"lat": -3.0, "lon": 7.25, "name": ""}]
o.history_step(+1)
assert o.entry.get() == "+14.50, -41.00 Torre"
o.history_step(+1)
assert o.entry.get() == "-3.00, +7.25"
o.history_step(-1)
o.history_step(-1)
assert o.entry.get() == ""
# entrada invalida no envia
o.entry.insert(0, "95, 10")
o.submit()
assert o.editing and "latitud fuera de rango" in o.status.cget("text")
o.cancel_edit()
# fuera de planeta / offline
o._on_coords(dict(base, lat=None, lon=None, on_planet=False, reason="no estas en un planeta", input_request_id=1))
assert o._layout == ["message"]
o._results.put(("offline", "x"))
o._drain()
assert o.message.cget("text") == "Esperando a NMS.py…"
# acerca de
o.show_about()
o.root.update()
about = [w for w in o.root.winfo_children() if w.winfo_class() == "Toplevel"][0]
texts = " ".join(c.cget("text") for c in about.winfo_children() if c.winfo_class() == "Label")
for needed in ("v0.0.3", "MightyZeus, originario de Nicaragua", "https://github.com/ImMightyZeus/Olympian-Planet-Coords",
               "https://paypal.me/ImMightyZeus", "Pangro Daer Barun D'Gross", "SIL OFL 1.1",
               "Divulgación sobre el uso de IA", "Olympian NMS Save Editor", "Otros proyectos"):
    assert needed in texts, needed
o.root.destroy()
print("OK overlay")
sys.exit(0)
