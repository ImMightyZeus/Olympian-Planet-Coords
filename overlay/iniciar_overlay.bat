@echo off
REM Olympian Planet Coords v0.0.3 - abre el recuadro (sin consola). Requiere Python 3.10-3.13 de python.org
REM Flecha suave: py -3 -m pip install -U pillow
start "" pyw -3 "%~dp0olympian_overlay.py" %*
