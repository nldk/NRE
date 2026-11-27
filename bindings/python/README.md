# NRE for Python

Pure-Python bindings (ctypes, no dependencies, nothing to compile) for NRE's C API, `libnre_c` 0.3
(`include/nre.h`). Every function in `nre.h` is wrapped under its renderer.h name in snake_case:
`NRE::createModel` / `nreCreateModel` is `nre.create_model`. See `include/renderer.h` for what each one does.

## Requirements

- Python 3.10 or newer.
- A built `libnre_c` 0.3 and the `librenderer` next to it (e.g. `cmake-build-debug/bin` after building the repo).
  The bindings check the library's version on load and refuse anything that isn't 0.3.x.

## Pointing it at libnre_c

The library is loaded on the first `nre` call:

- if `NRE_LIB_DIR` is set, from `$NRE_LIB_DIR/libnre_c.so.0.3` (`libnre_c.0.3.dylib` on macOS, `nre_c.dll` on Windows);
- otherwise by that name through the platform's normal search (`LD_LIBRARY_PATH`, the system library dirs, `PATH`
  on Windows).

If it can't be loaded, the call raises `nre.Error` with `code == nre.ErrorCode.LIBRARY_LOAD_FAILED`.

```sh
export NRE_LIB_DIR=/path/to/VulkanRenderer/cmake-build-debug/bin
```

Use it from the source tree (`sys.path`/`PYTHONPATH` pointing at `bindings/python`) or install it with
`pip install bindings/python`.

## Usage

```python
import nre

nre.set_render_config(800, 600, "hello", api_mode=nre.ApiMode.SCENE)
nre.init()
model = nre.create_model("box.glb")
car = nre.create_instance(model)
nre.create_scene()
nre.add_instance_to_scene(car)
nre.set_camera_position((0, 1, 3))
nre.camera_look_at((0, 0, 0))
while not nre.window_should_close():
    nre.poll_events()
    nre.render_frame()
nre.shutdown()
```

- **Errors:** a failed call raises `nre.Error`. It carries `.code`, an `nre.ErrorCode` (codes these bindings don't
  know come back as `ErrorCode.UNKNOWN`, with the raw value in `.raw_code`), and `.message`, the library's message.
- **Defaults:** these are plain keyword parameters: `set_render_config(width, height, title, mode=RENDER_3D,
  api_mode=SCENE, error_policy=RETURN, manual_frame_presentation_and_submission=False, resizable=True,
  show_fps_in_title=True)`, lights `color=(1, 1, 1)` with `intensity=3` (directional light, sun) or `10` (point
  light), `create_texture(path, srgb=True)`, `set_haze(visibility, height=300)`,
  `create_mesh_model(vertices, indices, material=None)` and `draw_model(model, transform=None)`.
- **Math:** `Vec2`, `Vec3`, `Quat` (identity `Quat()` = (0, 0, 0, 1)) and `Transform` are immutable NamedTuples
  that unpack like tuples. Vector parameters take any 3-sequence, e.g. `(0, 1, 0)`. `Mat4` is a 16-float tuple,
  column-major (`m[column * 4 + row]`, plus `.at(col, row)`, `.column(i)` and `.row(i)`).
- **Handles** (`Model`, `Instance`, ...) are small frozen values that compare by value. Nothing is destroyed
  automatically: call the matching `destroy_*()` yourself. `Model()` etc. is the empty handle, and it's falsy.
  Where the C API takes the empty handle, you can pass `None`: the textures of `create_material` and
  `set_material_base_texture`, and `set_skybox(None)`, which removes the skybox.
- **Your own geometry:** `create_mesh_model(vertices, indices)` takes `Vertex` values or `(position, normal, uv)`
  tuples (a `(0, 0, 0)` normal, the default, is computed from the triangles) and a sequence of ints. For big meshes,
  skip the per-vertex conversion: pass a ctypes array of `nre._ffi.NreVertex`, or any C-contiguous buffer of packed
  vertices (8 float32s each: position, normal, uv), e.g. `array.array("f", ...)` or a numpy float32 array of shape
  `(n, 8)`; and indices as a buffer of 32-bit ints (`array.array("I", ...)`, numpy uint32). Writable buffers are
  passed as they are, read-only ones copied in one go; the renderer copies the geometry either way.
- **Picking:** `pick_instance((x, y))` returns an `nre.RayHit(instance, distance, point)` or `None` if nothing
  is under that pixel.
- **Scene lighting:** `set_scene_lighting(nre.SceneLighting.LANDSCAPE)` (or `SMALL`, the default),
  `get_scene_lighting()`, `set_shadow_distance(meters)`, `set_haze(visibility, height=300)`, `set_exposure(e)`,
  `set_sky_color(color)` and `set_casts_shadows(instance, casts)`. Bad values (e.g. a negative shadow distance)
  raise `nre.Error` with `ErrorCode.INVALID_ARGUMENT`.
- **Threads:** GLFW needs the main thread. Call the setup, frame, window and input functions from it.
- **Raw layer:** `nre._ffi` has the ctypes structs and a prototype (argtypes/restype) for every function, if you
  need to call the C API directly.

Misuse checks (bad handles, wrong mode, ...) are only in a Debug `librenderer`; a Release one assumes the code was
tested against Debug. Note that with no `set_render_config()` call yet, the renderer has no error policy and
treats every error as Panic: the process aborts instead of raising.

## Changes from 0.2

- Targets `libnre_c` 0.3 (`libnre_c.so.0.3`); 0.2 libraries are refused.
- `create_texture(path, srgb=True)`: new `srgb` parameter. Existing calls still load colour (sRGB) textures; pass
  `srgb=False` for normal and metallic-roughness maps.
- New: `create_mesh_model`, `set_casts_shadows`, `set_scene_lighting`, `get_scene_lighting`, `set_shadow_distance`,
  `set_haze`, `set_exposure`, `set_sky_color`, `get_projection_matrix`, `pick_instance`; the types `Vertex`,
  `RayHit` and `SceneLighting`; and `ErrorCode.INVALID_ARGUMENT`.

## Example

`examples/orbit.py` is a port of `example/main.cpp`: the police car on the plains skybox, with an orbit camera
(drag with the left mouse button, scroll to zoom). Set `API_MODE` in the file to switch between scene mode and
function-draw mode.

```sh
cd bindings/python
NRE_LIB_DIR=../../cmake-build-debug/bin python3 examples/orbit.py
```

Assets are read from `<repo>/example/assets`, and the compiled shaders from `$NRE_SHADER_DIR` (default
`<repo>/cmake-build-debug/example/shaders`).

## Tests

These are headless: they open no window and need no GPU. They use plain `unittest`:

```sh
cd bindings/python
NRE_LIB_DIR=../../cmake-build-debug/bin python3 -m unittest -v
```
