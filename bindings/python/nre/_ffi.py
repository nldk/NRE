"""Raw ctypes declarations for libnre_c (include/nre.h), one prototype per function.

Nothing here converts or checks results: every function returns the raw NreErrorCode int. The Pythonic layer
in nre/__init__.py builds on this. The library is loaded on first use (see ``lib``), so importing this module
never fails.
"""

from __future__ import annotations

import ctypes
import os
import sys
import threading
from ctypes import POINTER, Structure, c_bool, c_char_p, c_float, c_int, c_uint32
from typing import Any

# The ABI these bindings target: nre.h of libnre_c 0.3.x.
ABI_MAJOR = 0
ABI_MINOR = 3

# Every C enum is kept 32-bit by its *_MAX_ENUM = 0x7FFFFFFF member, so int is the right ctypes type.
c_enum = c_int

# Error code returned when the library can't be loaded (NRE_ERROR_LIBRARY_LOAD_FAILED).
LIBRARY_LOAD_FAILED = 29


# ---------------------------------------------------------------------------
# Math types
# ---------------------------------------------------------------------------
class NreVec2(Structure):
    _fields_ = [("x", c_float), ("y", c_float)]


class NreVec3(Structure):
    _fields_ = [("x", c_float), ("y", c_float), ("z", c_float)]


class NreQuat(Structure):
    _fields_ = [("x", c_float), ("y", c_float), ("z", c_float), ("w", c_float)]


class NreMat4(Structure):
    _fields_ = [("m", c_float * 16)]  # column-major: m[column * 4 + row]


class NreVertex(Structure):
    """One vertex for nreCreateMeshModel. A normal of (0, 0, 0) is computed from the triangles using the vertex."""

    _fields_ = [("position", NreVec3), ("normal", NreVec3), ("uv", NreVec2)]


class NreTransform(Structure):
    _fields_ = [("position", NreVec3), ("rotation", NreQuat), ("scale", NreVec3)]


# ---------------------------------------------------------------------------
# Handles: one struct type per handle, like nre.h. {0, 0} is the empty handle.
# ---------------------------------------------------------------------------
class _NreHandle(Structure):
    _fields_ = [("index", c_uint32), ("generation", c_uint32)]


class NreShader(_NreHandle): pass
class NrePipeline(_NreHandle): pass
class NreModel(_NreHandle): pass
class NreInstance(_NreHandle): pass
class NreLight(_NreHandle): pass
class NreScene(_NreHandle): pass
class NreTexture(_NreHandle): pass
class NreMaterial(_NreHandle): pass
class NreCubemap(_NreHandle): pass


HANDLE_STRUCTS = (NreShader, NrePipeline, NreModel, NreInstance, NreLight, NreScene, NreTexture, NreMaterial,
                  NreCubemap)


class NreRayHit(Structure):
    """What nrePickInstance hit: the instance, the distance along the ray (world units), the world-space point."""

    _fields_ = [("instance", NreInstance), ("distance", c_float), ("point", NreVec3)]


# ---------------------------------------------------------------------------
# Prototypes: name -> (restype, argtypes). Same order as nre.h.
# ---------------------------------------------------------------------------
_E = c_enum  # NreErrorCode
_P = POINTER

PROTOTYPES: dict[str, tuple[Any, list[Any]]] = {
    # Errors and version
    "nreGetLastErrorMessage": (c_char_p, []),
    "nreErrorCodeToString": (c_char_p, [c_enum]),
    "nreGetVersion": (None, [_P(c_uint32), _P(c_uint32), _P(c_uint32)]),
    # Setup and frames
    "nreSetRenderConfig": (_E, [c_enum, c_enum, c_enum, c_bool, c_int, c_int, c_char_p, c_bool, c_bool]),
    "nreInit": (_E, []),
    "nreShutdown": (_E, []),
    "nreRenderFrame": (_E, []),
    "nreEndFrame": (_E, []),
    "nrePresentFrame": (_E, []),
    "nreSubmitFrame": (_E, []),
    "nreWindowShouldClose": (_E, [_P(c_bool)]),
    "nrePollEvents": (_E, []),
    # Shaders and pipelines
    "nreCreateShader": (_E, [c_char_p, c_enum, _P(NreShader)]),
    "nreDestroyShader": (_E, [NreShader]),
    "nreCreatePipeline": (_E, [NreShader, NreShader, _P(NrePipeline)]),
    "nreDestroyPipeline": (_E, [NrePipeline]),
    "nreBindPipeline": (_E, [NrePipeline]),
    # Models and instances
    "nreCreateModel": (_E, [c_char_p, _P(NreModel)]),
    "nreCreateMeshModel": (_E, [_P(NreVertex), c_uint32, _P(c_uint32), c_uint32, NreMaterial, _P(NreModel)]),
    "nreDestroyModel": (_E, [NreModel]),
    "nreCreateInstance": (_E, [NreModel, _P(NreInstance)]),
    "nreDestroyInstance": (_E, [NreInstance]),
    "nreSetInstanceTransform": (_E, [NreInstance, _P(NreTransform)]),
    "nreGetInstanceTransform": (_E, [NreInstance, _P(NreTransform)]),
    "nreGetInstanceMatrix": (_E, [NreInstance, _P(NreMat4)]),
    "nreTranslateInstance": (_E, [NreInstance, NreVec3]),
    "nreRotateInstance": (_E, [NreInstance, c_float, NreVec3]),
    "nreScaleInstance": (_E, [NreInstance, NreVec3]),
    "nreSetVisibility": (_E, [NreInstance, c_bool]),
    "nreSetCastsShadows": (_E, [NreInstance, c_bool]),
    "nreDrawModel": (_E, [NreModel, _P(NreTransform)]),  # transform may be NULL
    "nreDrawInstance": (_E, [NreInstance]),
    # Scenes
    "nreCreateScene": (_E, [_P(NreScene)]),
    "nreDestroyScene": (_E, [NreScene]),
    "nreBindScene": (_E, [NreScene]),
    "nreGetBoundScene": (_E, [_P(NreScene)]),
    "nreAddInstanceToScene": (_E, [NreInstance]),
    "nreRemoveInstanceFromScene": (_E, [NreInstance]),
    # Textures and materials
    "nreCreateTexture": (_E, [c_char_p, c_bool, _P(NreTexture)]),
    "nreDestroyTexture": (_E, [NreTexture]),
    "nreCreateMaterial": (_E, [NreTexture, NreTexture, NreTexture, _P(NreMaterial)]),
    "nreDestroyMaterial": (_E, [NreMaterial]),
    "nreSetMaterialBaseTexture": (_E, [NreMaterial, NreTexture]),
    "nreGetModelMaterials": (_E, [NreModel, _P(c_uint32), _P(NreMaterial)]),  # materials may be NULL
    "nreSetModelMaterial": (_E, [NreModel, NreMaterial]),
    "nreReplaceModelMaterial": (_E, [NreModel, c_uint32, NreMaterial]),
    # Lights and the sun
    "nreCreateDirectionalLight": (_E, [NreVec3, NreVec3, c_float, _P(NreLight)]),
    "nreCreatePointLight": (_E, [NreVec3, NreVec3, c_float, _P(NreLight)]),
    "nreDestroyLight": (_E, [NreLight]),
    "nreAddLightToScene": (_E, [NreLight]),
    "nreRemoveLightFromScene": (_E, [NreLight]),
    "nreSetLightPosition": (_E, [NreLight, NreVec3]),
    "nreSetLightDirection": (_E, [NreLight, NreVec3]),
    "nreSetLightColor": (_E, [NreLight, NreVec3, c_float]),
    "nreSetSun": (_E, [NreVec3, NreVec3, c_float]),
    "nreSetSunDirection": (_E, [NreVec3]),
    "nreGetSunDirection": (_E, [_P(NreVec3)]),
    # Scene lighting, shadows, haze, exposure and sky colour
    "nreSetSceneLighting": (_E, [c_enum]),
    "nreGetSceneLighting": (_E, [_P(c_enum)]),
    "nreSetShadowDistance": (_E, [c_float]),
    "nreSetHaze": (_E, [c_float, c_float]),
    "nreSetExposure": (_E, [c_float]),
    "nreSetSkyColor": (_E, [NreVec3]),
    # Cubemaps and the skybox
    "nreCreateCubemap": (_E, [c_char_p, c_char_p, c_char_p, c_char_p, c_char_p, c_char_p, _P(NreCubemap)]),
    "nreDestroyCubemap": (_E, [NreCubemap]),
    "nreSetSkybox": (_E, [NreCubemap]),
    # Camera
    "nreSetCameraPosition": (_E, [NreVec3]),
    "nreGetCameraPosition": (_E, [_P(NreVec3)]),
    "nreMoveCamera": (_E, [NreVec3]),
    "nreMoveCameraRelative": (_E, [NreVec3]),
    "nreSetCameraRotation": (_E, [c_float, c_float]),
    "nreRotateCamera": (_E, [c_float, c_float]),
    "nreCameraLookAt": (_E, [NreVec3]),
    "nreGetCameraForward": (_E, [_P(NreVec3)]),
    "nreGetViewMatrix": (_E, [_P(NreMat4)]),
    "nreGetProjectionMatrix": (_E, [_P(NreMat4)]),
    "nrePickInstance": (_E, [NreVec2, _P(c_bool), _P(NreRayHit)]),
    # Timing
    "nreGetFPS": (_E, [_P(c_float)]),
    "nreGetDeltaTime": (_E, [_P(c_float)]),
    # Input
    "nreIsMouseButtonDown": (_E, [c_enum, _P(c_bool)]),
    "nreIsMouseButtonPressed": (_E, [c_enum, _P(c_bool)]),
    "nreIsMouseButtonReleased": (_E, [c_enum, _P(c_bool)]),
    "nreGetMouseDelta": (_E, [_P(NreVec2)]),
    "nreGetScrollDelta": (_E, [_P(c_float)]),
    "nreGetMousePosition": (_E, [_P(NreVec2)]),
    "nreSetCursorMode": (_E, [c_enum]),
    "nreIsKeyDown": (_E, [c_enum, _P(c_bool)]),
    "nreIsKeyPressed": (_E, [c_enum, _P(c_bool)]),
    "nreIsKeyReleased": (_E, [c_enum, _P(c_bool)]),
}


# ---------------------------------------------------------------------------
# Loading
# ---------------------------------------------------------------------------
class LoadError(Exception):
    """The library couldn't be loaded, lacks a function, or has the wrong ABI version."""


def library_name() -> str:
    """The platform's file name for libnre_c 0.3."""
    if sys.platform == "win32":
        return "nre_c.dll"
    if sys.platform == "darwin":
        return "libnre_c.0.3.dylib"
    return "libnre_c.so.0.3"


def library_path() -> str:
    """$NRE_LIB_DIR/<name> if NRE_LIB_DIR is set, else the bare name for the platform's normal search."""
    lib_dir = os.environ.get("NRE_LIB_DIR")
    return os.path.join(lib_dir, library_name()) if lib_dir else library_name()


def load(path: str | None = None) -> ctypes.CDLL:
    """Loads libnre_c, checks that its version is 0.3.x and sets every prototype."""
    path = path or library_path()
    try:
        dll = ctypes.CDLL(path)
    except OSError as e:
        raise LoadError(f"could not load {path!r} ({e}); set NRE_LIB_DIR to the directory holding "
                        f"{library_name()}") from e
    # The version first, so another version's library is reported as such rather than as a missing function.
    try:
        get_version = dll.nreGetVersion
    except AttributeError as e:
        raise LoadError(f"{path!r} has no function nreGetVersion: not libnre_c") from e
    get_version.restype, get_version.argtypes = PROTOTYPES["nreGetVersion"]
    major, minor, patch = c_uint32(), c_uint32(), c_uint32()
    get_version(ctypes.byref(major), ctypes.byref(minor), ctypes.byref(patch))
    if (major.value, minor.value) != (ABI_MAJOR, ABI_MINOR):
        raise LoadError(f"{path!r} is libnre_c {major.value}.{minor.value}.{patch.value}, but these bindings "
                        f"target {ABI_MAJOR}.{ABI_MINOR}.x")
    for name, (restype, argtypes) in PROTOTYPES.items():
        try:
            fn = getattr(dll, name)
        except AttributeError as e:
            raise LoadError(f"{path!r} has no function {name}: not the libnre_c these bindings target") from e
        fn.restype = restype
        fn.argtypes = argtypes
    return dll


class _LazyLib:
    """Stands in for the CDLL: the first attribute lookup loads the library and copies its functions here,
    so later lookups are plain attribute reads."""

    def __init__(self) -> None:
        self._lock = threading.Lock()

    def __getattr__(self, name: str) -> Any:
        if name not in PROTOTYPES:
            raise AttributeError(name)
        with self._lock:
            if name not in self.__dict__:
                dll = load()
                for fn_name in PROTOTYPES:
                    self.__dict__[fn_name] = getattr(dll, fn_name)
                self.__dict__["_dll"] = dll
        return self.__dict__[name]


# The library's functions, e.g. ``lib.nreInit()``. Raises LoadError on first use if it can't be loaded.
lib: Any = _LazyLib()
