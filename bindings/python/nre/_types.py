"""Value types of the bindings: enums, the error, math types and handles. See include/nre.h and renderer.h."""

from __future__ import annotations

import math
from dataclasses import dataclass
from enum import IntEnum
from typing import Any, ClassVar, Iterable, NamedTuple, Sequence, Union

from . import _ffi

__all__ = [
    "ErrorCode", "ShaderType", "RenderMode", "ApiMode", "SceneLighting", "RenderErrorPolicy", "MouseButton",
    "CursorMode", "Key", "Error", "Version", "Vec2", "Vec3", "Quat", "Mat4", "Transform", "Vertex", "Vec2Like",
    "Vec3Like", "QuatLike", "Shader", "Pipeline", "Model", "Instance", "Light", "Scene", "Texture", "Material",
    "Cubemap", "RayHit",
]


# ---------------------------------------------------------------------------
# Enums. Same values as nre.h.
# ---------------------------------------------------------------------------
class ErrorCode(IntEnum):
    """NreErrorCode. A value these bindings don't know (from a newer library) becomes UNKNOWN."""

    OK = 0
    UNSUPPORTED_GPU = 1
    CREATING_VULKAN_INSTANCE = 2
    CREATING_SWAPCHAIN = 3
    RECREATING_SWAPCHAIN = 4
    CREATING_PIPELINE = 5
    WINDOW_CREATION_FAILED = 6
    FILE_NOT_FOUND = 7
    WRONG_MODE = 8
    NOT_INITIALIZED = 9
    ALREADY_INITIALIZED = 10
    INVALID_HANDLE = 11  # empty, destroyed, or never created
    UNSUPPORTED_FORMAT = 12
    MODEL_LOAD_FAILED = 13
    FAILED_TO_CREATE_ALLOCATOR = 14
    BUFFER_CREATION_FAILED = 15
    UPLOAD_FAILED = 16
    NO_PIPELINE_BOUND = 17
    NO_SCENE_BOUND = 18
    CREATING_TEXTURE = 19
    IMAGE_CREATION_FAILED = 20
    IMAGE_VIEW_CREATION_FAILED = 21
    INDEX_OUT_OF_RANGE = 22
    IMPOSSIBLE_TYPE_FOR_PER_FRAME_BUFFER = 23
    BUFFER_WITH_SIZE_NULL = 24
    SIZE_OF_BUFFER_TOO_BIG = 25
    DESCRIPTOR_CREATION_FAILED = 26
    SHADER_CREATION_FAILED = 27
    UNKNOWN = 28  # also: a C++ exception inside the renderer
    LIBRARY_LOAD_FAILED = 29
    INVALID_ARGUMENT = 30  # e.g. a negative shadow distance, or picking in RENDER_2D

    @classmethod
    def _missing_(cls, value: object) -> ErrorCode:
        return cls.UNKNOWN


class ShaderType(IntEnum):
    FRAGMENT = 0
    VERTEX = 1
    GEOMETRY = 2
    COMPUTE = 3


class RenderMode(IntEnum):
    RENDER_3D = 0
    RENDER_2D = 1


class ApiMode(IntEnum):
    """SCENE: render_frame() draws the bound scene. FUNCTION_DRAW: render_frame(), draw_*() calls, end_frame()."""

    SCENE = 0
    FUNCTION_DRAW = 1


class SceneLighting(IntEnum):
    """A scene's lighting mode (set_scene_lighting); see renderer.h's SceneLighting."""

    SMALL = 0  # default: cubemap or sky colour, short sharp shadows, no haze
    LANDSCAPE = 1  # procedural sky from the sun, far cascaded shadows, haze, auto exposure


class RenderErrorPolicy(IntEnum):
    IGNORE = 0
    RETURN = 1
    PANIC = 2


class MouseButton(IntEnum):
    LEFT = 0
    RIGHT = 1
    MIDDLE = 2


class CursorMode(IntEnum):
    NORMAL = 0
    HIDDEN = 1  # invisible over the window, but moves and leaves it as usual
    LOCKED = 2  # hidden and kept in the window, unlimited movement: for mouse-look cameras


class Key(IntEnum):
    """Physical keys, named after the US layout. The values are GLFW's key codes."""

    SPACE = 32
    APOSTROPHE = 39
    COMMA = 44
    MINUS = 45
    PERIOD = 46
    SLASH = 47
    ZERO = 48
    ONE = 49
    TWO = 50
    THREE = 51
    FOUR = 52
    FIVE = 53
    SIX = 54
    SEVEN = 55
    EIGHT = 56
    NINE = 57
    SEMICOLON = 59
    EQUAL = 61
    A = 65
    B = 66
    C = 67
    D = 68
    E = 69
    F = 70
    G = 71
    H = 72
    I = 73
    J = 74
    K = 75
    L = 76
    M = 77
    N = 78
    O = 79
    P = 80
    Q = 81
    R = 82
    S = 83
    T = 84
    U = 85
    V = 86
    W = 87
    X = 88
    Y = 89
    Z = 90
    LEFT_BRACKET = 91
    BACKSLASH = 92
    RIGHT_BRACKET = 93
    GRAVE_ACCENT = 96
    ESCAPE = 256
    ENTER = 257
    TAB = 258
    BACKSPACE = 259
    INSERT = 260
    DELETE = 261
    RIGHT = 262
    LEFT = 263
    DOWN = 264
    UP = 265
    PAGE_UP = 266
    PAGE_DOWN = 267
    HOME = 268
    END = 269
    CAPS_LOCK = 280
    SCROLL_LOCK = 281
    NUM_LOCK = 282
    PRINT_SCREEN = 283
    PAUSE = 284
    F1 = 290
    F2 = 291
    F3 = 292
    F4 = 293
    F5 = 294
    F6 = 295
    F7 = 296
    F8 = 297
    F9 = 298
    F10 = 299
    F11 = 300
    F12 = 301
    KP_0 = 320
    KP_1 = 321
    KP_2 = 322
    KP_3 = 323
    KP_4 = 324
    KP_5 = 325
    KP_6 = 326
    KP_7 = 327
    KP_8 = 328
    KP_9 = 329
    KP_DECIMAL = 330
    KP_DIVIDE = 331
    KP_MULTIPLY = 332
    KP_SUBTRACT = 333
    KP_ADD = 334
    KP_ENTER = 335
    KP_EQUAL = 336
    LEFT_SHIFT = 340
    LEFT_CONTROL = 341
    LEFT_ALT = 342
    LEFT_SUPER = 343
    RIGHT_SHIFT = 344
    RIGHT_CONTROL = 345
    RIGHT_ALT = 346
    RIGHT_SUPER = 347
    MENU = 348


# ---------------------------------------------------------------------------
# Errors
# ---------------------------------------------------------------------------
class Error(Exception):
    """A failed NRE call: ``code`` is the ErrorCode, ``message`` what nreGetLastErrorMessage() said right after.

    ``raw_code`` is the integer the library returned; it differs from ``code`` only when the library returned a
    code these bindings don't know (then ``code`` is ErrorCode.UNKNOWN).
    """

    code: ErrorCode
    message: str
    raw_code: int

    def __init__(self, code: int, message: str) -> None:
        self.raw_code = int(code)
        self.code = ErrorCode(self.raw_code)
        self.message = message
        super().__init__(self.code, message)

    def __str__(self) -> str:
        return f"{self.code.name}: {self.message}" if self.message else self.code.name

    def __repr__(self) -> str:
        return f"nre.Error({self.code!r}, {self.message!r})"


class Version(NamedTuple):
    major: int
    minor: int
    patch: int


# ---------------------------------------------------------------------------
# Math types. Immutable tuples: they unpack like (x, y, z) and compare by value.
# Parameters take any sequence of the right length, so plain tuples and lists work too.
# ---------------------------------------------------------------------------
class Vec2(NamedTuple):
    x: float = 0.0
    y: float = 0.0

    def __add__(self, other: Sequence[float]) -> Vec2:  # type: ignore[override]
        return Vec2(self.x + other[0], self.y + other[1])

    def __sub__(self, other: Sequence[float]) -> Vec2:
        return Vec2(self.x - other[0], self.y - other[1])

    def __mul__(self, s: float) -> Vec2:  # type: ignore[override]
        return Vec2(self.x * s, self.y * s)

    __rmul__ = __mul__  # type: ignore[assignment]

    def __neg__(self) -> Vec2:
        return Vec2(-self.x, -self.y)


class Vec3(NamedTuple):
    x: float = 0.0
    y: float = 0.0
    z: float = 0.0

    def __add__(self, other: Sequence[float]) -> Vec3:  # type: ignore[override]
        return Vec3(self.x + other[0], self.y + other[1], self.z + other[2])

    def __sub__(self, other: Sequence[float]) -> Vec3:
        return Vec3(self.x - other[0], self.y - other[1], self.z - other[2])

    def __mul__(self, s: float) -> Vec3:  # type: ignore[override]
        return Vec3(self.x * s, self.y * s, self.z * s)

    __rmul__ = __mul__  # type: ignore[assignment]

    def __neg__(self) -> Vec3:
        return Vec3(-self.x, -self.y, -self.z)

    def length(self) -> float:
        return math.sqrt(self.x * self.x + self.y * self.y + self.z * self.z)


class Quat(NamedTuple):
    """x, y, z, w. ``Quat()`` is the identity (0, 0, 0, 1)."""

    x: float = 0.0
    y: float = 0.0
    z: float = 0.0
    w: float = 1.0

    @classmethod
    def from_axis_angle(cls, axis: Sequence[float], angle: float) -> Quat:
        """Rotation by ``angle`` radians around ``axis`` (normalized here)."""
        ax, ay, az = axis
        n = math.sqrt(ax * ax + ay * ay + az * az)
        s = math.sin(angle / 2) / n
        return cls(ax * s, ay * s, az * s, math.cos(angle / 2))


Vec2Like = Union[Vec2, Sequence[float]]
Vec3Like = Union[Vec3, Sequence[float]]
QuatLike = Union[Quat, Sequence[float]]


class Mat4(tuple):  # tuple[float, ...] of length 16
    """16 floats, column-major like glm and GLSL: ``m[column * 4 + row]``. ``Mat4()`` is the identity."""

    __slots__ = ()

    def __new__(cls, values: Iterable[float] | None = None) -> Mat4:
        if values is None:
            values = (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0)
        m = super().__new__(cls, (float(v) for v in values))
        if len(m) != 16:
            raise ValueError(f"Mat4 needs 16 values, got {len(m)}")
        return m

    def at(self, column: int, row: int) -> float:
        return self[column * 4 + row]

    def column(self, column: int) -> tuple[float, float, float, float]:
        c = column * 4
        return (self[c], self[c + 1], self[c + 2], self[c + 3])

    def row(self, row: int) -> tuple[float, float, float, float]:
        return (self[row], self[4 + row], self[8 + row], self[12 + row])

    def __repr__(self) -> str:
        return f"Mat4({tuple(self)!r})"


class Transform(NamedTuple):
    """Applied as scale, then rotation, then translation. ``Transform()`` is the identity.

    The fields accept any sequences, e.g. ``Transform(position=(0, 1, 0))``.
    """

    position: Vec3Like = Vec3()
    rotation: QuatLike = Quat()
    scale: Vec3Like = Vec3(1.0, 1.0, 1.0)


class Vertex(NamedTuple):
    """One vertex for create_mesh_model. A normal of (0, 0, 0) (the default) is computed from the triangles that
    use the vertex. The fields accept any sequences, e.g. ``Vertex((0, 1, 0), uv=(0.5, 1))``."""

    position: Vec3Like = Vec3()
    normal: Vec3Like = Vec3()
    uv: Vec2Like = Vec2()


# ---------------------------------------------------------------------------
# Handles: small values (index + generation) to copy around; destroy them explicitly with destroy_*().
# The default ``Model()`` etc. is the empty handle; it's falsy. Handles of different types never compare equal.
# ---------------------------------------------------------------------------
@dataclass(frozen=True, slots=True)
class _Handle:
    index: int = 0
    generation: int = 0

    _c: ClassVar[Any]

    @classmethod
    def empty(cls) -> Any:
        return cls()

    @property
    def is_empty(self) -> bool:
        return self.index == 0 and self.generation == 0

    def __bool__(self) -> bool:
        return not self.is_empty

    def _to_c(self) -> Any:
        return self._c(self.index, self.generation)

    @classmethod
    def _from_c(cls, h: Any) -> Any:
        return cls(h.index, h.generation)


@dataclass(frozen=True, slots=True)
class Shader(_Handle):
    _c = _ffi.NreShader


@dataclass(frozen=True, slots=True)
class Pipeline(_Handle):
    _c = _ffi.NrePipeline


@dataclass(frozen=True, slots=True)
class Model(_Handle):
    _c = _ffi.NreModel


@dataclass(frozen=True, slots=True)
class Instance(_Handle):
    _c = _ffi.NreInstance


@dataclass(frozen=True, slots=True)
class Light(_Handle):
    _c = _ffi.NreLight


@dataclass(frozen=True, slots=True)
class Scene(_Handle):
    _c = _ffi.NreScene


@dataclass(frozen=True, slots=True)
class Texture(_Handle):
    _c = _ffi.NreTexture


@dataclass(frozen=True, slots=True)
class Material(_Handle):
    _c = _ffi.NreMaterial


@dataclass(frozen=True, slots=True)
class Cubemap(_Handle):
    _c = _ffi.NreCubemap


class RayHit(NamedTuple):
    """What pick_instance hit: the instance, the distance from the camera along the ray (world units) and the
    world-space point."""

    instance: Instance
    distance: float
    point: Vec3
