"""Genera la guia visual animada de Olympian Planet Coords (MP4 + GIF, español e inglés).

No es un dibujo inventado: usa el recuadro REAL (overlay/olympian_overlay.py, con sus fuentes y su
flecha de Pillow) y la logica REAL del DLL (core_harness compilado de dll/src/core.cpp). Solo el
paisaje, el marcador del HUD y los rotulos son ilustracion.

Uso (Linux con Xvfb, ffmpeg y el banco de pruebas del DLL compilado):
    python docs/anim/make_guide.py <ruta core_harness> <carpeta_salida> [es|en|both]

Olympian Planet Coords — Copyright © 2026 MightyZeus · Licencia GPL-3.0
"""

from __future__ import annotations

import importlib.util
import json
import math
import os
import random
import subprocess
import sys
import tempfile
import threading
import time

from PIL import Image, ImageDraw, ImageFilter, ImageFont, ImageGrab

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
W, H = 1280, 720
FPS = 12
DISPLAY = os.environ.get("OPC_DISPLAY", ":97")
FONT_TXT = os.path.join(ROOT, "overlay", "fonts", "ShareTechMono-Regular.ttf")
LOGO = os.path.join(ROOT, "docs", "logo.png")
CENTER = (1000.0, -2000.0, 500.0)
RADIUS = 50000.0
SCALE = 1.35  # el recuadro se amplia para que se lea en el video
FOV = 70.0  # grados de campo horizontal del "juego" ilustrado
START = (12.00, -45.00)
DEST = (12.40, -44.55, "Base Zeus")
TYPED = "+12.40, -44.55 Base Zeus"

CAPTIONS = {
    "es": {
        "title": "Cómo funciona", "sub": "Coordenadas en vivo y destinos por planeta para No Man's Sky",
        "s1": "Coordenadas del planeta en vivo, arriba a la izquierda",
        "s2": "Pulsa F9 en el juego (o haz clic en el recuadro)",
        "s3": "Escribe  latitud, longitud  y un nombre (opcional)",
        "s4": "Enter: destino guardado y marcador en el HUD del juego",
        "s5": "La flecha 3D apunta al destino; distancia y rumbo en vivo",
        "s6": "Ve hacia el marcador: la distancia baja sola",
        "s7": "A 50 m llegaste: el destino se quita solo",
        "s8": "Clic derecho: tamaño, tecla (F5–F11), idioma y más",
        "end1": "Instalación", "end2": "1.  Copia winmm.dll en la carpeta Binaries del juego",
        "end3": "2.  Abre el recuadro (OlympianPlanetCoords-Overlay.exe)",
        "end4": "3.  Juega desde Steam. Sin Python ni NMS.py.",
        "end5": "Un destino por planeta · historial de 10 · español / inglés",
        "credit": "MightyZeus · GPL-3.0 · github.com/ImMightyZeus · paypal.me/ImMightyZeus",
        "key": "tecla",
    },
    "en": {
        "title": "How it works", "sub": "Live coordinates and per-planet destinations for No Man's Sky",
        "s1": "Live planet coordinates in the top-left corner",
        "s2": "Press F9 in game (or click the box)",
        "s3": "Type  latitude, longitude  and an optional name",
        "s4": "Enter: destination saved and a marker on the game's HUD",
        "s5": "The 3D arrow points to it; live distance and bearing",
        "s6": "Head for the marker: the distance counts down",
        "s7": "Within 50 m you've arrived: the destination clears itself",
        "s8": "Right-click: size, hotkey (F5–F11), language and more",
        "end1": "Installation", "end2": "1.  Copy winmm.dll into the game's Binaries folder",
        "end3": "2.  Open the box (OlympianPlanetCoords-Overlay.exe)",
        "end4": "3.  Play from Steam. No Python or NMS.py.",
        "end5": "One destination per planet · history of 10 · English / Spanish",
        "credit": "MightyZeus · GPL-3.0 · github.com/ImMightyZeus · paypal.me/ImMightyZeus",
        "key": "key",
    },
}


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    sys.modules[name] = m
    spec.loader.exec_module(m)
    return m


# ---------------------------------------------------------------- geometria (igual que las pruebas)
def unit(lat, lon):
    la, lo = math.radians(lat), math.radians(lon)
    return (math.cos(la) * math.sin(lo), math.sin(la), math.cos(la) * math.cos(lo))  # convencion 0 (atan2_x_z)


def sub(a, b):
    return tuple(x - y for x, y in zip(a, b))


def norm(a):
    n = math.sqrt(sum(x * x for x in a))
    return tuple(x / n for x in a)


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def tangent_frame(lat, lon, heading_deg):
    """heading: 0 = norte, 90 = este. Devuelve (right, up, at) del jugador."""
    up = unit(lat, lon)
    north = norm(sub(unit(lat + 0.01, lon), unit(lat, lon)))
    east = norm(cross(north, up))
    if sum(e * d for e, d in zip(east, sub(unit(lat, lon + 0.01), unit(lat, lon)))) < 0:
        east = tuple(-x for x in east)
    h = math.radians(heading_deg)
    at = norm(tuple(math.cos(h) * n + math.sin(h) * e for n, e in zip(north, east)))
    right = norm(cross(at, up))  # yaw > 0 = a la derecha, como en el juego
    return right, up, at


class Harness:
    def __init__(self, exe, data):
        self.p = subprocess.Popen([exe, "core", data], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True,
                                  encoding="utf-8")
        self.lock = threading.Lock()

    def ask(self, obj):
        with self.lock:
            self.p.stdin.write(json.dumps(obj) + "\n")
            self.p.stdin.flush()
            return json.loads(self.p.stdout.readline())

    def close(self):
        self.p.stdin.close()
        self.p.wait(timeout=5)


class SimClient:
    """Cliente del overlay conectado al nucleo real del DLL (en vez de HTTP)."""

    def __init__(self, h):
        self.h = h
        self.latest = None

    def get_coords(self):
        if self.latest is None:
            raise OSError("sin datos")
        return self.latest

    def place_marker(self, lat, lon, name):
        return self.h.ask({"put": "marker", "query": {"lat": str(lat), "lon": str(lon), "name": name}})

    def clear_marker(self):
        return self.h.ask({"put": "clear", "query": {}})

    def set_lang(self, code):
        return self.h.ask({"put": "lang", "query": {"code": code}})

    def set_hotkey(self, key):
        return self.h.ask({"put": "hotkey", "query": {"key": key}})


# ---------------------------------------------------------------- paisaje ilustrado
def make_panorama(seed=11):
    """Panorama de 360° (4 pantallas de ancho) de un planeta ficticio."""
    rnd = random.Random(seed)
    pw = W * 4
    img = Image.new("RGB", (pw, H))
    d = ImageDraw.Draw(img)
    top, mid, low = (22, 44, 78), (214, 120, 92), (246, 196, 132)
    hz = int(H * 0.56)
    for y in range(hz):
        t = y / hz
        c = tuple(int(top[i] + (mid[i] - top[i]) * min(1, t * 1.25)) if t < 0.8 else
                  int(mid[i] + (low[i] - mid[i]) * (t - 0.8) / 0.2) for i in range(3))
        d.line([(0, y), (pw, y)], fill=c)
    for _ in range(260):  # estrellas tenues arriba
        x, y = rnd.randrange(pw), rnd.randrange(int(hz * 0.45))
        v = rnd.randrange(150, 255)
        d.point((x, y), fill=(v, v, v))
    # planetas en el cielo
    for cx, cy, r, col in ((int(pw * 0.18), 120, 70, (120, 170, 210)), (int(pw * 0.62), 90, 38, (230, 215, 190)),
                           (int(pw * 0.83), 150, 24, (160, 120, 200))):
        sky = Image.new("RGBA", (r * 4, r * 4), (0, 0, 0, 0))
        sd = ImageDraw.Draw(sky)
        sd.ellipse((r, r, r * 3, r * 3), fill=col + (255,))
        sd.ellipse((r + r // 3, r - r // 6, r * 3 + r // 3, r * 3 - r // 6), fill=(0, 0, 0, 0))
        img.paste(sky, (cx - 2 * r, cy - 2 * r), sky.filter(ImageFilter.GaussianBlur(1)))
    # capas de montañas
    for layer, (col, base, amp) in enumerate(((112, 76, 96), (84, 58, 82), (58, 44, 66))):
        pts = [(0, H)]
        y0 = hz - 40 + layer * 38
        for x in range(0, pw + 40, 40):
            y = y0 - amp * (0.5 + 0.5 * math.sin(x / (230 + layer * 70) + layer * 2.1)) - rnd.randrange(0, 18)
            pts.append((x, y))
        pts.append((pw, H))
        d.polygon(pts, fill=col)
    # suelo
    g0 = hz + 70
    for y in range(g0, H):
        t = (y - g0) / (H - g0)
        d.line([(0, y), (pw, y)], fill=(int(70 + 40 * t), int(96 + 30 * t), int(70 + 10 * t)))
    for _ in range(140):  # flora alienigena
        x = rnd.randrange(pw)
        y = rnd.randrange(g0 + 10, H)
        s = 4 + (y - g0) // 14
        col = rnd.choice([(150, 220, 170), (240, 140, 180), (120, 200, 230)])
        d.ellipse((x - s, y - 2 * s, x + s, y), fill=col)
        d.line([(x, y), (x, y + s)], fill=(40, 60, 40), width=2)
    return img.filter(ImageFilter.GaussianBlur(0.6))


def view_of(pano, heading):
    pw = pano.width
    x = int((heading % 360) / 360 * pw)
    if x + W <= pw:
        return pano.crop((x, 0, x + W, H))
    out = Image.new("RGB", (W, H))
    out.paste(pano.crop((x, 0, pw, H)), (0, 0))
    out.paste(pano.crop((0, 0, W - (pw - x), H)), (pw - x, 0))
    return out


def draw_marker(img, yaw, pitch, dist_m, label, font):
    """Marcador generico del HUD (ilustracion), en la direccion que da el nucleo."""
    if abs(yaw) > FOV / 2 + 5:
        return
    d = ImageDraw.Draw(img, "RGBA")
    x = W / 2 + yaw / (FOV / 2) * (W / 2)
    y = H * 0.56 - pitch / (FOV / 2) * (W / 2) * 0.9 + 10
    s = 22
    d.polygon([(x, y - s), (x + s, y), (x, y + s), (x - s, y)], outline=(255, 255, 255, 255), fill=(127, 212, 255, 120))
    d.polygon([(x, y - s / 2.5), (x + s / 2.5, y), (x, y + s / 2.5), (x - s / 2.5, y)], fill=(255, 255, 255, 255))
    dist = f"{dist_m / 1000:.1f} km" if dist_m >= 1000 else f"{int(dist_m)} m"
    for i, (txt, col) in enumerate(((label, (255, 255, 255)), (dist, (127, 212, 255)))):
        tw = d.textlength(txt, font=font)
        ty = y + s + 8 + i * 26
        d.text((x - tw / 2 + 1, ty + 1), txt, font=font, fill=(0, 0, 0, 160))
        d.text((x - tw / 2, ty), txt, font=font, fill=col)


def caption(img, step, text, font_big, font_small):
    band = Image.new("RGBA", (W, 74), (8, 12, 16, 200))
    img.paste(band, (0, H - 74), band)
    d = ImageDraw.Draw(img)
    if step:
        d.ellipse((28, H - 56, 66, H - 18), fill=(242, 177, 52))
        n = str(step)
        d.text((47 - d.textlength(n, font=font_big) / 2, H - 52), n, font=font_big, fill=(16, 20, 24))
    d.text((84 if step else 32, H - 52), text, font=font_big, fill=(232, 241, 242))


def keycap(img, label, sub, font_big, font_small, alpha=1.0):
    d = ImageDraw.Draw(img, "RGBA")
    x, y, w, h = W - 190, H - 200, 120, 96
    a = int(255 * alpha)
    d.rounded_rectangle((x + 4, y + 6, x + w + 4, y + h + 6), 14, fill=(0, 0, 0, int(120 * alpha)))
    d.rounded_rectangle((x, y, x + w, y + h), 14, fill=(232, 241, 242, a), outline=(242, 177, 52, a), width=4)
    tw = d.textlength(label, font=font_big)
    d.text((x + w / 2 - tw / 2, y + 22), label, font=font_big, fill=(16, 20, 24, a))
    tw = d.textlength(sub, font=font_small)
    d.text((x + w / 2 - tw / 2, y + h + 12), sub, font=font_small, fill=(232, 241, 242, a))


# ---------------------------------------------------------------- guion
def render(lang, harness_exe, out_dir):
    ov = load(os.path.join(ROOT, "overlay", "olympian_overlay.py"), f"olympian_overlay_{lang}")
    C = CAPTIONS[lang]
    work = tempfile.mkdtemp(prefix=f"opc_anim_{lang}_")
    settings = os.path.join(work, "overlay.json")
    with open(settings, "w", encoding="utf-8") as f:
        json.dump({"lang": lang, "size": "grande"}, f)
    h = Harness(harness_exe, os.path.join(work, "datos.json"))
    h.ask({"put": "lang", "query": {"code": lang}})
    client = SimClient(h)
    os.environ["DISPLAY"] = DISPLAY
    o = ov.Overlay(client, 16, 16, settings_file=settings)
    o.root.attributes("-alpha", 1.0)
    pano = make_panorama()
    f_big = ImageFont.truetype(FONT_TXT, 26)
    f_small = ImageFont.truetype(FONT_TXT, 17)
    f_title = ImageFont.truetype(FONT_TXT, 54)
    f_mark = ImageFont.truetype(FONT_TXT, 22)
    logo = Image.open(LOGO).convert("RGBA")

    frames_dir = os.path.join(work, "frames")
    os.makedirs(frames_dir)
    state = {"t": 0.0, "lat": START[0], "lon": START[1], "heading": 20.0, "n": 0}

    def pump():
        o.root.update_idletasks()
        o.root.update()

    def game_frame(**extra):
        r, u, a = tangent_frame(state["lat"], state["lon"], state["heading"])
        pos = tuple(CENTER[i] + unit(state["lat"], state["lon"])[i] * RADIUS for i in range(3))
        f = {"pos": pos, "center": list(CENTER), "t": state["t"], "location": 3, "right": r, "up": u, "at": a}
        f.update(extra)
        data = h.ask(f)
        client.latest = data
        o._on_coords(data)
        o._drain()
        return data

    def arrow_step(data, dt):
        d = (data or {}).get("destination") or {}
        if d and d.get("yaw") is not None:
            yaw, pitch, mt = float(d["yaw"]), float(d.get("pitch") or 0), 0.0
        else:
            yaw, pitch, mt = o.arrow_yaw, o.arrow_pitch, 1.0
        k = 1 - math.exp(-dt * 12)
        o.arrow_yaw = ov.approach_angle(o.arrow_yaw, yaw, k)
        o.arrow_pitch += (pitch - o.arrow_pitch) * k
        step = dt / ov.MORPH_SECONDS
        o.morph = min(mt, o.morph + step) if o.morph < mt else max(mt, o.morph - step)

    def grab_overlay():
        pump()
        time.sleep(0.03)
        pump()
        fx, fy = o.frame.winfo_rootx(), o.frame.winfo_rooty()
        fw, fh = o.frame.winfo_width(), o.frame.winfo_height()
        shot = ImageGrab.grab(bbox=(fx, fy, fx + fw, fy + fh), xdisplay=DISPLAY)
        return shot, (fx, fy, fw, fh)

    def compose(data, step, text, marker=True, key=None, menu=False, dim=0.0):
        bg = view_of(pano, state["heading"]).convert("RGBA")
        d = (data or {}).get("destination") or {}
        if marker and d and d.get("yaw") is not None and data.get("on_planet"):
            draw_marker(bg, float(d["yaw"]), float(d.get("pitch") or 0), d["distance_m"], d.get("name") or "",
                        f_mark)
        box, (fx, fy, fw, fh) = grab_overlay()

        def sc(v):
            return int(round(v * SCALE))

        bg.alpha_composite(box.convert("RGBA").resize((sc(fw), sc(fh)), Image.LANCZOS), (fx, fy))
        size = sc(max(24, fh))
        arrow = ov.render_arrow_image(ov.arrow_geometry(o.arrow_yaw, o.arrow_pitch, o.morph), size)
        bg.alpha_composite(arrow, (fx + sc(fw) + 8, fy))
        if menu:
            mx, my = o.menu.winfo_rootx(), o.menu.winfo_rooty()
            mw, mh = o.menu.winfo_width(), o.menu.winfo_height()
            if mw > 1:
                shot = ImageGrab.grab(bbox=(mx, my, mx + mw, my + mh), xdisplay=DISPLAY).convert("RGBA")
                bg.alpha_composite(shot.resize((sc(mw), sc(mh)), Image.LANCZOS),
                                   (fx + sc(mx - fx), fy + sc(my - fy)))
        if key:
            keycap(bg, key[0], C["key"], f_title, f_small, key[1])
        if dim:
            bg.alpha_composite(Image.new("RGBA", (W, H), (0, 0, 0, int(255 * dim))))
        if text:
            caption(bg, step, text, f_big, f_small)
        return bg.convert("RGB")

    def save(img):
        img.save(os.path.join(frames_dir, f"f{state['n']:05d}.png"))
        state["n"] += 1

    def tick(dt=1 / FPS):
        state["t"] += dt

    def title_card(seconds):
        for i in range(int(seconds * FPS)):
            bg = view_of(pano, state["heading"]).convert("RGBA")
            bg.alpha_composite(Image.new("RGBA", (W, H), (6, 10, 14, 200)))
            a = min(1.0, i / 8)
            lg = logo.resize((220, 220), Image.LANCZOS)
            lg.putalpha(lg.getchannel("A").point(lambda v: int(v * a)))
            bg.alpha_composite(lg, (W // 2 - 110, 110))
            d = ImageDraw.Draw(bg)
            for txt, fnt, y, col in (("OLYMPIAN PLANET COORDS", f_title, 350, (242, 177, 52)),
                                     (C["title"], f_big, 425, (232, 241, 242)), (C["sub"], f_small, 470, (136, 153, 166))):
                d.text((W / 2 - d.textlength(txt, font=fnt) / 2, y), txt, font=fnt, fill=col)
            save(bg.convert("RGB"))
            tick()

    # 0. portada; el nucleo ya ve al jugador (para que este "estable" al poner el destino)
    data = game_frame()
    title_card(2.5)
    # 1. coordenadas en vivo (caminando)
    for _ in range(int(3.0 * FPS)):
        state["lat"] += 0.0006
        state["lon"] -= 0.0004
        state["heading"] -= 0.6
        data = game_frame()
        arrow_step(data, 1 / FPS)
        save(compose(data, 1, C["s1"]))
        tick()
    # 2. F9: el nucleo cuenta la pulsacion y el overlay abre el campo
    h.ask({"key": True, "focused": True})
    for i in range(int(1.4 * FPS)):
        data = game_frame()
        save(compose(data, 2, C["s2"], key=("F9", min(1.0, (i + 1) / 4))))
        tick()
    h.ask({"key": False})
    # 3. escribir
    o.entry.delete(0, "end")
    per = max(1, int(2.6 * FPS) // len(TYPED))
    for ch in TYPED:
        o.entry.insert("end", ch)
        for _ in range(per):
            data = game_frame()
            save(compose(data, 3, C["s3"]))
            tick()
    for _ in range(int(0.6 * FPS)):
        save(compose(game_frame(), 3, C["s3"]))
        tick()
    # 4. Enter (camino real: submit -> PUT /marker al nucleo)
    o.submit()
    for _ in range(int(2.8 * FPS)):
        data = game_frame()
        arrow_step(data, 1 / FPS)
        save(compose(data, 4, C["s4"]))
        tick()
    # 5. girar hacia el destino
    d = data.get("destination") or {}
    target_heading = state["heading"] + float(d.get("yaw") or 0)
    n = int(3.0 * FPS)
    h0 = state["heading"]
    for i in range(n):
        k = (1 - math.cos(math.pi * (i + 1) / n)) / 2
        state["heading"] = h0 + (target_heading - h0) * k
        data = game_frame()
        arrow_step(data, 1 / FPS)
        save(compose(data, 5, C["s5"]))
        tick()
    # 6. avanzar hacia el destino (en exonave: rapido)
    for _ in range(int(9.0 * FPS)):
        d = data.get("destination") or {}
        if not d:
            break
        state["heading"] += float(d.get("yaw") or 0) * 0.3
        hd = math.radians(state["heading"])
        speed = min(0.006, max(0.0004, d["distance_m"] / RADIUS * 57.3 * 0.09))
        state["lat"] += math.cos(hd) * speed
        state["lon"] += math.sin(hd) * speed / math.cos(math.radians(state["lat"]))
        data = game_frame()
        arrow_step(data, 1 / FPS)
        save(compose(data, 6, C["s6"]))
        tick()
    # 7. llegada: el nucleo quita el destino y la flecha se vuelve esfera
    for _ in range(int(3.0 * FPS)):
        data = game_frame()
        arrow_step(data, 1 / FPS)
        save(compose(data, 7, C["s7"]))
        tick()
    # 8. menu del clic derecho (el menu real del recuadro)
    class Ev:
        x_root = o.frame.winfo_rootx() + 60
        y_root = o.frame.winfo_rooty() + o.frame.winfo_height() + 4

    o.menu.post(Ev.x_root, Ev.y_root)
    for _ in range(int(3.0 * FPS)):
        data = game_frame()
        arrow_step(data, 1 / FPS)
        save(compose(data, 8, C["s8"], menu=True))
        tick()
    o.menu.unpost()
    # 9. cierre
    for i in range(int(4.5 * FPS)):
        data = game_frame()
        bg = compose(data, 0, "", dim=min(0.82, i / 6 * 0.82)).convert("RGBA")
        dr = ImageDraw.Draw(bg)
        a = min(255, i * 40)
        lines = [(C["end1"], f_title, (242, 177, 52)), (C["end2"], f_big, (232, 241, 242)),
                 (C["end3"], f_big, (232, 241, 242)), (C["end4"], f_big, (232, 241, 242)),
                 (C["end5"], f_small, (127, 212, 255)), (C["credit"], f_small, (136, 153, 166))]
        y = 150
        for txt, fnt, col in lines:
            dr.text((W / 2 - dr.textlength(txt, font=fnt) / 2, y), txt, font=fnt, fill=col + (a,))
            y += 80 if fnt is f_title else 52
        save(bg.convert("RGB"))
        tick()

    o.root.destroy()
    h.close()
    os.makedirs(out_dir, exist_ok=True)
    base = os.path.join(out_dir, f"olympian-planet-coords-guide-{lang}")
    pattern = os.path.join(frames_dir, "f%05d.png")
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-framerate", str(FPS), "-i", pattern, "-c:v", "libx264",
                    "-preset", "veryfast", "-crf", "20", "-pix_fmt", "yuv420p", "-movflags", "+faststart",
                    base + ".mp4"], check=True)
    palette = os.path.join(work, "pal.png")
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-framerate", str(FPS), "-i", pattern, "-vf",
                    "scale=854:-1:flags=lanczos,palettegen=max_colors=128:stats_mode=diff", palette], check=True)
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-framerate", str(FPS), "-i", pattern, "-i", palette,
                    "-lavfi", "scale=854:-1:flags=lanczos[x];[x][1:v]paletteuse=dither=bayer:bayer_scale=4",
                    "-loop", "0", base + ".gif"], check=True)
    print(lang, state["n"], "frames ->", base + ".mp4", base + ".gif", "frames:", frames_dir)
    return frames_dir


if __name__ == "__main__":
    exe, out = sys.argv[1], sys.argv[2]
    langs = ["es", "en"] if (len(sys.argv) < 4 or sys.argv[3] == "both") else [sys.argv[3]]
    for lg in langs:
        render(lg, exe, out)
