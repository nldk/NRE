"""Python bindings for NRE, the Vulkan renderer, over its C API (libnre_c, include/nre.h).

Same functions as renderer.h in snake_case (NRE::createModel -> nre.create_model); see renderer.h for what each
one does. A failed call raises ``nre.Error`` with the ErrorCode and the library's message; values come back as
return values. Handles are plain values: destroy what you create with the matching destroy_*() call.

GLFW needs the main thread: call set_render_config/init, the frame functions and the input functions from it.

The library is loaded on the first call: from $NRE_LIB_DIR if set, else by name through the platform's normal
search (libnre_c.so.0.3 / libnre_c.0.3.dylib / nre_c.dll). It must be version 0.3.x.
"""

from __future__ import annotations

import ctypes
import os
from ctypes import byref, c_bool, c_float, c_int, c_uint32
from typing import Any, Optional, Sequence, Union

from . import _ffi
from ._types import (
    ApiMode, Cubemap, CursorMode, Error, ErrorCode, Instance, Key, Light, Mat4, Material, Model, MouseButton,
    Pipeline, Quat, QuatLike, RayHit, RenderErrorPolicy, RenderMode, Scene, SceneLighting, Shader, ShaderType,
    Texture, Transform, Vec2, Vec2Like, Vec3, Vec3Like, Version, Vertex,
)

__version__ = "0.3.0"

__all__ = [
    # types
    "ErrorCode", "ShaderType", "RenderMode", "ApiMode", "SceneLighting", "RenderErrorPolicy", "MouseButton",
    "CursorMode", "Key", "Error", "Version", "Vec2", "Vec3", "Quat", "Mat4", "Transform", "Vertex", "Vec2Like",
    "Vec3Like", "QuatLike", "Shader", "Pipeline", "Model", "Instance", "Light", "Scene", "Texture", "Material",
    "Cubemap", "RayHit", "StrPath", "VerticesLike", "IndicesLike",
    # errors and version
    "get_last_error_message", "error_code_to_string", "get_version",
    # setup and frames
    "set_render_config", "init", "shutdown", "render_frame", "end_frame", "present_frame", "submit_frame",
    "window_should_close", "poll_events",
    # shaders and pipelines
    "create_shader", "destroy_shader", "create_pipeline", "destroy_pipeline", "bind_pipeline",
    # models and instances
    "create_model", "create_mesh_model", "destroy_model", "create_instance", "destroy_instance",
    "set_instance_transform", "get_instance_transform", "get_instance_matrix", "translate_instance",
    "rotate_instance", "scale_instance", "set_visibility", "set_casts_shadows", "draw_model", "draw_instance",
    # scenes
    "create_scene", "destroy_scene", "bind_scene", "get_bound_scene", "add_instance_to_scene",
    "remove_instance_from_scene",
    # textures and materials
    "create_texture", "destroy_texture", "create_material", "destroy_material", "set_material_base_texture",
    "get_model_materials", "set_model_material", "replace_model_material",
    # lights and the sun
    "create_directional_light", "create_point_light", "destroy_light", "add_light_to_scene",
    "remove_light_from_scene", "set_light_position", "set_light_direction", "set_light_color", "set_sun",
    "set_sun_direction", "get_sun_direction",
    # scene lighting, shadows, haze, exposure and sky colour
    "set_scene_lighting", "get_scene_lighting", "set_shadow_distance", "set_haze", "set_exposure", "set_sky_color",
    # cubemaps and the skybox
    "create_cubemap", "destroy_cubemap", "set_skybox",
    # camera
    "set_camera_position", "get_camera_position", "move_camera", "move_camera_relative", "set_camera_rotation",
    "rotate_camera", "camera_look_at", "get_camera_forward", "get_view_matrix", "get_projection_matrix",
    "pick_instance",
    # timing
    "get_fps", "get_delta_time",
    # input
    "is_mouse_button_down", "is_mouse_button_pressed", "is_mouse_button_released", "get_mouse_delta",
    "get_scroll_delta", "get_mouse_position", "set_cursor_mode", "is_key_down", "is_key_pressed",
    "is_key_released",
]

StrPath = Union[str, "os.PathLike[str]", bytes]
# create_mesh_model's vertices: Vertex values or (position, normal, uv) tuples, or, without a per-vertex copy, a
# ctypes array of _ffi.NreVertex or any C-contiguous buffer of packed vertices (8 float32 each, e.g. a numpy
# float32 array of shape (n, 8)).
VerticesLike = Union[Sequence[Union[Vertex, Sequence[Any]]], "ctypes.Array[_ffi.NreVertex]", memoryview, Any]
# create_mesh_model's indices: ints, or a ctypes c_uint32 array or a buffer of 32-bit ints (array.array("I"), ...).
IndicesLike = Union[Sequence[int], "ctypes.Array[c_uint32]", memoryview, Any]


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
class _Lib:
    """Forwards to _ffi.lib, turning a load failure into nre.Error(LIBRARY_LOAD_FAILED)."""

    def __getattr__(self, name: str) -> Any:
        try:
            fn = getattr(_ffi.lib, name)
        except _ffi.LoadError as e:
            raise Error(ErrorCode.LIBRARY_LOAD_FAILED, str(e)) from e
        setattr(self, name, fn)  # later lookups skip __getattr__
        return fn


_C: Any = _Lib()


def _check(code: int) -> None:
    if code != 0:
        raise Error(code, _decode(_C.nreGetLastErrorMessage()))


def _decode(s: bytes | None) -> str:
    return s.decode("utf-8", "replace") if s else ""


def _path(p: StrPath) -> bytes:
    return os.fsencode(p)  # UTF-8 on Linux/macOS (keeps undecodable names) and on Windows since Python 3.6


def _vec3(v: Vec3Like) -> _ffi.NreVec3:
    try:
        x, y, z = v
    except (TypeError, ValueError):
        raise TypeError(f"expected a 3-component vector, got {v!r}") from None
    return _ffi.NreVec3(x, y, z)


def _quat(q: QuatLike) -> _ffi.NreQuat:
    try:
        x, y, z, w = q
    except (TypeError, ValueError):
        raise TypeError(f"expected a quaternion (x, y, z, w), got {q!r}") from None
    return _ffi.NreQuat(x, y, z, w)


def _transform(t: Transform | Sequence[Any]) -> _ffi.NreTransform:
    try:
        position, rotation, scale = t
    except (TypeError, ValueError):
        raise TypeError(f"expected a Transform (position, rotation, scale), got {t!r}") from None
    return _ffi.NreTransform(_vec3(position), _quat(rotation), _vec3(scale))


def _vec2(v: Vec2Like) -> _ffi.NreVec2:
    try:
        x, y = v
    except (TypeError, ValueError):
        raise TypeError(f"expected a 2-component vector, got {v!r}") from None
    return _ffi.NreVec2(x, y)


def _to_vec2(v: _ffi.NreVec2) -> Vec2:
    return Vec2(v.x, v.y)


def _to_vec3(v: _ffi.NreVec3) -> Vec3:
    return Vec3(v.x, v.y, v.z)


def _buffer(obj: Any) -> memoryview | None:
    """A memoryview of obj if it supports the buffer protocol (but isn't a str), else None."""
    if isinstance(obj, (str, ctypes.Array)):
        return None
    try:
        return memoryview(obj)
    except TypeError:
        return None


_FLOAT32_FORMATS = ("f", "@f", "=f", "<f")
_UINT32_FORMATS = ("I", "@I", "=I", "<I", "i", "@i", "=i", "<i", "L", "@L", "l", "@l")
_BYTE_FORMATS = ("B", "b", "c", "@B", "@b")


def _from_buffer(mv: memoryview, c_type: Any, what: str) -> Any:
    """A ctypes array of c_type over mv's bytes: shares writable memory, copies read-only memory in one go."""
    if not mv.c_contiguous:
        raise ValueError(f"{what}: the buffer must be C-contiguous")
    size = ctypes.sizeof(c_type)
    if mv.nbytes % size:
        raise ValueError(f"{what}: {mv.nbytes} bytes isn't a whole number of {size}-byte elements")
    raw = mv if mv.ndim == 1 and mv.format == "B" else mv.cast("B")
    array_type = c_type * (mv.nbytes // size)
    return array_type.from_buffer_copy(raw) if raw.readonly else array_type.from_buffer(raw)


def _vertex_array(vertices: VerticesLike) -> Any:
    if isinstance(vertices, ctypes.Array):
        if vertices._type_ is not _ffi.NreVertex:
            raise TypeError(f"vertices: expected a ctypes array of NreVertex, got one of {vertices._type_.__name__}")
        return vertices
    mv = _buffer(vertices)
    if mv is not None:
        if not (mv.itemsize == ctypes.sizeof(_ffi.NreVertex) or mv.format in _FLOAT32_FORMATS
                or mv.format in _BYTE_FORMATS):
            raise TypeError(f"vertices: a buffer needs float32 items (8 per vertex), 32-byte vertex items or "
                            f"bytes, got format {mv.format!r}")
        return _from_buffer(mv, _ffi.NreVertex, "vertices")
    vertices = vertices if isinstance(vertices, Sequence) else list(vertices)
    array = (_ffi.NreVertex * len(vertices))()
    for i, v in enumerate(vertices):
        try:
            position, normal, uv = v
        except (TypeError, ValueError):
            raise TypeError(f"vertices[{i}]: expected a Vertex (position, normal, uv), got {v!r}") from None
        array[i] = _ffi.NreVertex(_vec3(position), _vec3(normal), _vec2(uv))
    return array


def _index_array(indices: IndicesLike) -> Any:
    if isinstance(indices, ctypes.Array):
        if indices._type_ not in (c_uint32, ctypes.c_int32):
            raise TypeError(f"indices: expected a ctypes array of c_uint32, got one of {indices._type_.__name__}")
        return indices if indices._type_ is c_uint32 else (c_uint32 * len(indices)).from_buffer(indices)
    mv = _buffer(indices)
    if mv is not None:
        if not (mv.itemsize == 4 and mv.format in _UINT32_FORMATS):
            raise TypeError(f"indices: a buffer needs 32-bit integer items, got format {mv.format!r} "
                            f"({mv.itemsize} bytes each)")
        return _from_buffer(mv, c_uint32, "indices")
    indices = indices if isinstance(indices, Sequence) else list(indices)
    for i in indices:
        if not 0 <= i <= 0xFFFFFFFF:
            raise ValueError(f"indices: {i} doesn't fit in a uint32")
    return (c_uint32 * len(indices))(*indices)


def _scene_lighting(raw: int) -> SceneLighting:
    try:
        return SceneLighting(raw)
    except ValueError:
        raise Error(ErrorCode.UNKNOWN, f"the library returned SceneLighting {raw}, which these bindings don't "
                                       f"know") from None


def _texture(t: Texture | None) -> _ffi.NreTexture:
    return _ffi.NreTexture() if t is None else t._to_c()


def _get_bool(fn: Any, *args: Any) -> bool:
    out = c_bool()
    _check(fn(*args, byref(out)))
    return out.value


def _get_float(fn: Any) -> float:
    out = c_float()
    _check(fn(byref(out)))
    return out.value


def _get_vec3(fn: Any) -> Vec3:
    out = _ffi.NreVec3()
    _check(fn(byref(out)))
    return _to_vec3(out)


def _get_vec2(fn: Any) -> Vec2:
    out = _ffi.NreVec2()
    _check(fn(byref(out)))
    return _to_vec2(out)


# ---------------------------------------------------------------------------
# Errors and version
# ---------------------------------------------------------------------------
def get_last_error_message() -> str:
    """The message of the most recent failed call on this thread ("" if none failed yet).

    nre.Error already carries it; this is for completeness."""
    return _decode(_C.nreGetLastErrorMessage())


def error_code_to_string(code: ErrorCode | int) -> str:
    """"Ok", "FileNotFound", ...: the library's name for the code."""
    return _decode(_C.nreErrorCodeToString(int(code)))


def get_version() -> Version:
    """The loaded library's version."""
    major, minor, patch = c_uint32(), c_uint32(), c_uint32()
    _C.nreGetVersion(byref(major), byref(minor), byref(patch))
    return Version(major.value, minor.value, patch.value)


# ---------------------------------------------------------------------------
# Setup and frames (see renderer.h). Main thread only.
# ---------------------------------------------------------------------------
def set_render_config(width: int, height: int, title: str,
                      mode: RenderMode = RenderMode.RENDER_3D,
                      api_mode: ApiMode = ApiMode.SCENE,
                      error_policy: RenderErrorPolicy = RenderErrorPolicy.RETURN,
                      manual_frame_presentation_and_submission: bool = False,
                      resizable: bool = True,
                      show_fps_in_title: bool = True) -> None:
    """Call before init(). The window is width x height pixels with this title."""
    _check(_C.nreSetRenderConfig(int(mode), int(api_mode), int(error_policy),
                                 manual_frame_presentation_and_submission, width, height,
                                 title.encode("utf-8"), resizable, show_fps_in_title))


def init() -> None:
    """Opens the window and sets up Vulkan. Main thread only."""
    _check(_C.nreInit())


def shutdown() -> None:
    _check(_C.nreShutdown())


def render_frame() -> None:
    """SCENE: draws the bound scene and (unless manual presentation) submits and presents it.
    FUNCTION_DRAW: starts the frame; then draw_model()/draw_instance(), then end_frame()."""
    _check(_C.nreRenderFrame())


def end_frame() -> None:
    """FUNCTION_DRAW only: ends the frame render_frame() started and (unless manual) submits and presents it."""
    _check(_C.nreEndFrame())


def present_frame() -> None:
    _check(_C.nrePresentFrame())


def submit_frame() -> None:
    _check(_C.nreSubmitFrame())


def window_should_close() -> bool:
    return _get_bool(_C.nreWindowShouldClose)


def poll_events() -> None:
    """Processes window events and updates the input state. Main thread only."""
    _check(_C.nrePollEvents())


# ---------------------------------------------------------------------------
# Shaders and pipelines
# ---------------------------------------------------------------------------
def create_shader(path: StrPath, type: ShaderType) -> Shader:
    """Loads a compiled SPIR-V shader (.spv)."""
    out = _ffi.NreShader()
    _check(_C.nreCreateShader(_path(path), int(type), byref(out)))
    return Shader._from_c(out)


def destroy_shader(shader: Shader) -> None:
    _check(_C.nreDestroyShader(shader._to_c()))


def create_pipeline(vertex_shader: Shader, fragment_shader: Shader) -> Pipeline:
    out = _ffi.NrePipeline()
    _check(_C.nreCreatePipeline(vertex_shader._to_c(), fragment_shader._to_c(), byref(out)))
    return Pipeline._from_c(out)


def destroy_pipeline(pipeline: Pipeline) -> None:
    _check(_C.nreDestroyPipeline(pipeline._to_c()))


def bind_pipeline(pipeline: Pipeline) -> None:
    _check(_C.nreBindPipeline(pipeline._to_c()))


# ---------------------------------------------------------------------------
# Models and instances
# ---------------------------------------------------------------------------
def create_model(path: StrPath) -> Model:
    """Loads a model file (glTF, OBJ, ...). Draw it through instances (or draw_model in FUNCTION_DRAW mode)."""
    out = _ffi.NreModel()
    _check(_C.nreCreateModel(_path(path), byref(out)))
    return Model._from_c(out)


def create_mesh_model(vertices: VerticesLike, indices: IndicesLike, material: Material | None = None) -> Model:
    """A model from your own triangles (counter-clockwise front faces, indices into vertices), e.g. terrain.

    vertices: Vertex values or (position, normal, uv) tuples; vertices with a (0, 0, 0) normal get one computed
    from their triangles. For big meshes, pass a ctypes array of nre._ffi.NreVertex or a C-contiguous buffer of
    packed vertices (8 float32s each: position, normal, uv, e.g. a numpy float32 array of shape (n, 8)); indices
    likewise as ints or a buffer of 32-bit ints (array.array("I"), numpy uint32). Those aren't converted one
    element at a time. The geometry is copied. material None (or an empty handle) uses the default material."""
    v = _vertex_array(vertices)
    i = _index_array(indices)
    out = _ffi.NreModel()
    _check(_C.nreCreateMeshModel(v, len(v), i, len(i), _ffi.NreMaterial() if material is None else material._to_c(),
                                 byref(out)))
    return Model._from_c(out)


def destroy_model(model: Model) -> None:
    """Instances of a destroyed model stay valid but draw nothing."""
    _check(_C.nreDestroyModel(model._to_c()))


def create_instance(model: Model) -> Instance:
    """A copy of the model with its own transform."""
    out = _ffi.NreInstance()
    _check(_C.nreCreateInstance(model._to_c(), byref(out)))
    return Instance._from_c(out)


def destroy_instance(instance: Instance) -> None:
    """Also removes it from every scene it's in."""
    _check(_C.nreDestroyInstance(instance._to_c()))


def set_instance_transform(instance: Instance, transform: Transform | Sequence[Any]) -> None:
    t = _transform(transform)
    _check(_C.nreSetInstanceTransform(instance._to_c(), byref(t)))


def get_instance_transform(instance: Instance) -> Transform:
    out = _ffi.NreTransform()
    _check(_C.nreGetInstanceTransform(instance._to_c(), byref(out)))
    r = out.rotation
    return Transform(_to_vec3(out.position), Quat(r.x, r.y, r.z, r.w), _to_vec3(out.scale))


def get_instance_matrix(instance: Instance) -> Mat4:
    out = _ffi.NreMat4()
    _check(_C.nreGetInstanceMatrix(instance._to_c(), byref(out)))
    return Mat4(out.m)


def translate_instance(instance: Instance, offset: Vec3Like) -> None:
    _check(_C.nreTranslateInstance(instance._to_c(), _vec3(offset)))


def rotate_instance(instance: Instance, angle: float, axis: Vec3Like) -> None:
    """Angle is in radians."""
    _check(_C.nreRotateInstance(instance._to_c(), angle, _vec3(axis)))


def scale_instance(instance: Instance, factor: Vec3Like) -> None:
    _check(_C.nreScaleInstance(instance._to_c(), _vec3(factor)))


def set_visibility(instance: Instance, visible: bool) -> None:
    """A hidden instance stays in its scenes but isn't drawn."""
    _check(_C.nreSetVisibility(instance._to_c(), visible))


def set_casts_shadows(instance: Instance, casts_shadows: bool) -> None:
    """Whether the instance casts sun shadows (it still receives them). Default True; e.g. off for grass."""
    _check(_C.nreSetCastsShadows(instance._to_c(), casts_shadows))


def draw_model(model: Model, transform: Transform | Sequence[Any] | None = None) -> None:
    """FUNCTION_DRAW only, between render_frame() and end_frame(). None: the identity transform."""
    if transform is None:
        _check(_C.nreDrawModel(model._to_c(), None))
    else:
        t = _transform(transform)
        _check(_C.nreDrawModel(model._to_c(), byref(t)))


def draw_instance(instance: Instance) -> None:
    """FUNCTION_DRAW only. Uses the instance's transform; a hidden instance isn't drawn."""
    _check(_C.nreDrawInstance(instance._to_c()))


# ---------------------------------------------------------------------------
# Scenes
# ---------------------------------------------------------------------------
def create_scene() -> Scene:
    out = _ffi.NreScene()
    _check(_C.nreCreateScene(byref(out)))
    return Scene._from_c(out)


def destroy_scene(scene: Scene) -> None:
    _check(_C.nreDestroyScene(scene._to_c()))


def bind_scene(scene: Scene) -> None:
    _check(_C.nreBindScene(scene._to_c()))


def get_bound_scene() -> Scene:
    out = _ffi.NreScene()
    _check(_C.nreGetBoundScene(byref(out)))
    return Scene._from_c(out)


def add_instance_to_scene(instance: Instance) -> None:
    """Adds to the bound scene. Adding an instance that's already in it does nothing."""
    _check(_C.nreAddInstanceToScene(instance._to_c()))


def remove_instance_from_scene(instance: Instance) -> None:
    _check(_C.nreRemoveInstanceFromScene(instance._to_c()))


# ---------------------------------------------------------------------------
# Textures and materials. None (or an empty handle) for a texture means the default texture.
# ---------------------------------------------------------------------------
def create_texture(path: StrPath, srgb: bool = True) -> Texture:
    """Loads an image file (PNG, JPEG, ...). srgb=True for colour images (base colour); False for data maps
    (normal and metallic-roughness maps)."""
    out = _ffi.NreTexture()
    _check(_C.nreCreateTexture(_path(path), srgb, byref(out)))
    return Texture._from_c(out)


def destroy_texture(texture: Texture) -> None:
    """Give the materials that use it another texture first: they keep pointing at it."""
    _check(_C.nreDestroyTexture(texture._to_c()))


def create_material(base_texture: Texture | None = None, metallic_texture: Texture | None = None,
                    normal_texture: Texture | None = None) -> Material:
    """metallic_texture and normal_texture are stored but not used yet (see renderer.h)."""
    out = _ffi.NreMaterial()
    _check(_C.nreCreateMaterial(_texture(base_texture), _texture(metallic_texture), _texture(normal_texture),
                                byref(out)))
    return Material._from_c(out)


def destroy_material(material: Material) -> None:
    """Models still using it fall back to the default material."""
    _check(_C.nreDestroyMaterial(material._to_c()))


def set_material_base_texture(material: Material, texture: Texture | None) -> None:
    """Waits for the GPU to finish the frames in flight, so don't call it every frame."""
    _check(_C.nreSetMaterialBaseTexture(material._to_c(), _texture(texture)))


def get_model_materials(model: Model) -> list[Material]:
    """The model's material slots, in the file's material order. Empty for a model without materials."""
    m = model._to_c()
    count = c_uint32()
    _check(_C.nreGetModelMaterials(m, byref(count), None))
    if count.value == 0:
        return []
    array = (_ffi.NreMaterial * count.value)()
    _check(_C.nreGetModelMaterials(m, byref(count), array))
    return [Material._from_c(array[i]) for i in range(count.value)]


def set_model_material(model: Model, material: Material) -> None:
    """Makes every mesh of the model use material, replacing all of its slots."""
    _check(_C.nreSetModelMaterial(model._to_c(), material._to_c()))


def replace_model_material(model: Model, index: int, material: Material) -> None:
    """Replaces slot ``index`` (an index into get_model_materials)."""
    _check(_C.nreReplaceModelMaterial(model._to_c(), index, material._to_c()))


# ---------------------------------------------------------------------------
# Lights and the sun. direction is the way the light travels, e.g. (0, -1, 0) shines straight down.
# ---------------------------------------------------------------------------
def create_directional_light(direction: Vec3Like, color: Vec3Like = (1.0, 1.0, 1.0),
                             intensity: float = 3.0) -> Light:
    out = _ffi.NreLight()
    _check(_C.nreCreateDirectionalLight(_vec3(direction), _vec3(color), intensity, byref(out)))
    return Light._from_c(out)


def create_point_light(position: Vec3Like, color: Vec3Like = (1.0, 1.0, 1.0), intensity: float = 10.0) -> Light:
    """Falls off with 1/distance²."""
    out = _ffi.NreLight()
    _check(_C.nreCreatePointLight(_vec3(position), _vec3(color), intensity, byref(out)))
    return Light._from_c(out)


def destroy_light(light: Light) -> None:
    """Also removes it from every scene it's in."""
    _check(_C.nreDestroyLight(light._to_c()))


def add_light_to_scene(light: Light) -> None:
    """Adds to the bound scene. Adding a light that's already in it does nothing."""
    _check(_C.nreAddLightToScene(light._to_c()))


def remove_light_from_scene(light: Light) -> None:
    _check(_C.nreRemoveLightFromScene(light._to_c()))


def set_light_position(light: Light, position: Vec3Like) -> None:
    """Only used by point lights."""
    _check(_C.nreSetLightPosition(light._to_c(), _vec3(position)))


def set_light_direction(light: Light, direction: Vec3Like) -> None:
    """Only used by directional lights."""
    _check(_C.nreSetLightDirection(light._to_c(), _vec3(direction)))


def set_light_color(light: Light, color: Vec3Like, intensity: float) -> None:
    _check(_C.nreSetLightColor(light._to_c(), _vec3(color), intensity))


def set_sun(direction: Vec3Like, color: Vec3Like = (1.0, 1.0, 1.0), intensity: float = 3.0) -> None:
    """The bound scene's sun: one directional light kept apart from the others (see renderer.h)."""
    _check(_C.nreSetSun(_vec3(direction), _vec3(color), intensity))


def set_sun_direction(direction: Vec3Like) -> None:
    """Keeps the color and intensity, e.g. for a day/night cycle."""
    _check(_C.nreSetSunDirection(_vec3(direction)))


def get_sun_direction() -> Vec3:
    return _get_vec3(_C.nreGetSunDirection)


# ---------------------------------------------------------------------------
# Scene lighting, shadows, haze, exposure and sky colour, on the bound scene (see renderer.h). Defaults:
# SMALL: shadows 20 m, haze off. LANDSCAPE: shadows 500 m, haze 8000 m / 300 m. Exposure 1, sky (0.1, 0.3, 0.6).
# ---------------------------------------------------------------------------
def set_scene_lighting(lighting: SceneLighting) -> None:
    """Also resets the shadow distance, haze and sky to the mode's defaults."""
    _check(_C.nreSetSceneLighting(int(lighting)))


def get_scene_lighting() -> SceneLighting:
    out = c_int()
    _check(_C.nreGetSceneLighting(byref(out)))
    return _scene_lighting(out.value)


def set_shadow_distance(meters: float) -> None:
    """How far from the camera the sun casts shadows. 0 turns sun shadows off; negative raises INVALID_ARGUMENT."""
    _check(_C.nreSetShadowDistance(meters))


def set_haze(visibility: float, height: float = 300.0) -> None:
    """visibility: metres at which the haze hides almost everything, 0 = off. height: the haze layer's
    thickness (metres above y = 0), greater than 0."""
    _check(_C.nreSetHaze(visibility, height))


def set_exposure(exposure: float) -> None:
    """Greater than 0; 2 is twice as bright."""
    _check(_C.nreSetExposure(exposure))


def set_sky_color(color: Vec3Like) -> None:
    """The background without a skybox (SMALL), linear 0..1."""
    _check(_C.nreSetSkyColor(_vec3(color)))


# ---------------------------------------------------------------------------
# Cubemaps and the skybox
# ---------------------------------------------------------------------------
def create_cubemap(right: StrPath, left: StrPath, top: StrPath, bottom: StrPath, front: StrPath,
                   back: StrPath) -> Cubemap:
    """Six square images of the same size: right (+X), left (-X), top (+Y), bottom (-Y), front (+Z), back (-Z)."""
    out = _ffi.NreCubemap()
    _check(_C.nreCreateCubemap(_path(right), _path(left), _path(top), _path(bottom), _path(front), _path(back),
                               byref(out)))
    return Cubemap._from_c(out)


def destroy_cubemap(cubemap: Cubemap) -> None:
    """Waits for the GPU to finish the frames in flight. Scenes using it go back to having no skybox."""
    _check(_C.nreDestroyCubemap(cubemap._to_c()))


def set_skybox(cubemap: Cubemap | None) -> None:
    """The bound scene's sky (3D only). None removes it."""
    _check(_C.nreSetSkybox(_ffi.NreCubemap() if cubemap is None else cubemap._to_c()))


# ---------------------------------------------------------------------------
# Camera. Angles are in radians.
# ---------------------------------------------------------------------------
def set_camera_position(position: Vec3Like) -> None:
    _check(_C.nreSetCameraPosition(_vec3(position)))


def get_camera_position() -> Vec3:
    return _get_vec3(_C.nreGetCameraPosition)


def move_camera(offset: Vec3Like) -> None:
    _check(_C.nreMoveCamera(_vec3(offset)))


def move_camera_relative(offset: Vec3Like) -> None:
    _check(_C.nreMoveCameraRelative(_vec3(offset)))


def set_camera_rotation(yaw: float, pitch: float) -> None:
    _check(_C.nreSetCameraRotation(yaw, pitch))


def rotate_camera(yaw_delta: float, pitch_delta: float) -> None:
    _check(_C.nreRotateCamera(yaw_delta, pitch_delta))


def camera_look_at(target: Vec3Like) -> None:
    _check(_C.nreCameraLookAt(_vec3(target)))


def get_camera_forward() -> Vec3:
    return _get_vec3(_C.nreGetCameraForward)


def get_view_matrix() -> Mat4:
    out = _ffi.NreMat4()
    _check(_C.nreGetViewMatrix(byref(out)))
    return Mat4(out.m)


def get_projection_matrix() -> Mat4:
    """Vulkan conventions: clip-space +Y down, reverse-Z with an infinite far plane (see renderer.h)."""
    out = _ffi.NreMat4()
    _check(_C.nreGetProjectionMatrix(byref(out)))
    return Mat4(out.m)


def pick_instance(window_pos: Vec2Like) -> Optional[RayHit]:
    """The nearest visible instance of the bound scene under a pixel (get_mouse_position units), or None.
    RENDER_3D only."""
    hit = c_bool()
    out = _ffi.NreRayHit()
    _check(_C.nrePickInstance(_vec2(window_pos), byref(hit), byref(out)))
    if not hit.value:
        return None
    return RayHit(Instance._from_c(out.instance), out.distance, _to_vec3(out.point))


# ---------------------------------------------------------------------------
# Timing
# ---------------------------------------------------------------------------
def get_fps() -> float:
    """Frames per second, averaged over the last half second."""
    return _get_float(_C.nreGetFPS)


def get_delta_time() -> float:
    """Seconds between the last two presented frames."""
    return _get_float(_C.nreGetDeltaTime)


# ---------------------------------------------------------------------------
# Input, as of the last poll_events() call. Pressed / released are true for one frame. Main thread only.
# ---------------------------------------------------------------------------
def is_mouse_button_down(button: MouseButton) -> bool:
    return _get_bool(_C.nreIsMouseButtonDown, int(button))


def is_mouse_button_pressed(button: MouseButton) -> bool:
    return _get_bool(_C.nreIsMouseButtonPressed, int(button))


def is_mouse_button_released(button: MouseButton) -> bool:
    return _get_bool(_C.nreIsMouseButtonReleased, int(button))


def get_mouse_delta() -> Vec2:
    """How far the cursor moved, in pixels. +x is right, +y is down."""
    return _get_vec2(_C.nreGetMouseDelta)


def get_scroll_delta() -> float:
    """Scroll wheel steps. Positive means scrolling up / away from you."""
    return _get_float(_C.nreGetScrollDelta)


def get_mouse_position() -> Vec2:
    """Pixels from the window's top-left corner."""
    return _get_vec2(_C.nreGetMousePosition)


def set_cursor_mode(mode: CursorMode) -> None:
    _check(_C.nreSetCursorMode(int(mode)))


def is_key_down(key: Key) -> bool:
    return _get_bool(_C.nreIsKeyDown, int(key))


def is_key_pressed(key: Key) -> bool:
    return _get_bool(_C.nreIsKeyPressed, int(key))


def is_key_released(key: Key) -> bool:
    return _get_bool(_C.nreIsKeyReleased, int(key))
