"""Simulacion: mod real + uvicorn real en :5000 + 'hilo del juego' (60 fps) que llama al hook
cGcPlayerHUD.Update. Si hay destino, el jugador camina hacia el (para ver distancia y llegada)."""
import math
import runpy
import sys
import threading
import time

sys.argv = ["x"]
g = runpy.run_path("test_mod.py", run_name="sim")
mod, GAME, at = g["mod"], g["GAME"], g["at"]
import uvicorn  # noqa: E402
from pymhf.core.http_api import CleanableAPIRouter, create_app  # noqa: E402

m = g["make_mod"]()
app = create_app()
r = CleanableAPIRouter(m._mod_name, prefix=f"/{m._mod_name}")
r.load_routes(m._http_endpoints)
app.include_router(r)
pos = [12.0, -45.0]
SPEED = float(sys.argv[1]) if len(sys.argv) > 1 else 0.02  # grados por frame hacia el destino


def game_thread():
    while True:
        d = m.http_coords.get("destination")
        if d:
            dl, dn = d["lat"] - pos[0], d["lon"] - pos[1]
            n = math.hypot(dl, dn)
            step = min(SPEED, n)
            if n > 0:
                pos[0] += dl / n * step
                pos[1] += dn / n * step
        at(*pos)
        m.on_hud_update(None, 0.016)
        time.sleep(1 / 60)


threading.Thread(target=game_thread, daemon=True).start()
uvicorn.run(app, host="127.0.0.1", port=5000, log_level="warning")
