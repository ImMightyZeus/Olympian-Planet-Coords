"""Stubs SOLO para pruebas en Linux: sustituye paquetes exclusivos de Windows
(cyminhook, pymem, pywin32, keyboard, ...) para poder importar pymhf/nmspy reales y
comprobar layouts de structs y firmas. No se distribuye con el mod."""
import importlib.abc, importlib.machinery, sys, types

WIN_ONLY = {"cyminhook", "pymem", "win32gui", "win32api", "win32process", "win32con", "win32ui",
            "pywintypes", "pywinctl", "keyboard", "pyrun_injected", "iced_x86", "win32com", "dearpygui", "winreg", "win32event", "win32file"}


class _Dummy:
    def __init__(self, *a, **k): pass
    def __call__(self, *a, **k): return _Dummy()
    def __getattr__(self, n):
        if n[:1].isupper():
            import ctypes
            return type(n, (ctypes.Structure,), {"_fields_": []})
        return _Dummy()
    def __mro_entries__(self, bases): return (object,)


class _StubModule(types.ModuleType):
    def __getattr__(self, name):
        if name.startswith("__"):
            raise AttributeError(name)
        if name.isupper() or name.startswith("SYSTEM"):
            import ctypes
            return type(name, (ctypes.Structure,), {"_fields_": []})
        if name[:1].isupper():
            return type(name, (), {"__init__": lambda self, *a, **k: None,
                                   "__getattr__": lambda self, n: _Dummy()})
        return _Dummy()


class _Finder(importlib.abc.MetaPathFinder, importlib.abc.Loader):
    def find_spec(self, fullname, path, target=None):
        if fullname.split(".")[0] in WIN_ONLY:
            return importlib.machinery.ModuleSpec(fullname, self, is_package=True)
    def create_module(self, spec):
        m = _StubModule(spec.name); m.__path__ = []; return m
    def exec_module(self, module): pass


sys.meta_path.insert(0, _Finder())

import ctypes as _ct
if not hasattr(_ct, "windll"):
    _ct.windll = _Dummy()
    _ct.WinDLL = _Dummy
    _ct.WINFUNCTYPE = _ct.CFUNCTYPE

# pymhf.core.process usa _winapi (solo Windows): se sustituye entero.
_proc = types.ModuleType("pymhf.core.process")
_proc.start_process = lambda *a, **k: None
sys.modules["pymhf.core.process"] = _proc
import pymhf.core  # noqa: E402,F401  (parchea ctypes._Pointer como en el runtime real)
