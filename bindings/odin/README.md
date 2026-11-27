# NRE for Odin

Odin bindings for NRE over its C API (`include/nre.h`, `libnre_c`). Package `nre` in `nre/`:

- `nre/raw.odin`: every function of `nre.h` with its C name minus the prefix (`nreCreateModel` -> `nre.CreateModel`), exact C signatures.
- `nre/nre.odin`: the same functions in Odin style (`nre.create_model`), with defaults from `renderer.h`.
- `nre/types.odin`: enums, math types and handles.

```odin
import nre "path/to/bindings/odin/nre"

run :: proc() -> (err: nre.Error_Code) {
	nre.set_render_config(800, 600, "demo") or_return  // mode, api_mode, error_policy, ... have defaults
	nre.init() or_return
	defer nre.shutdown()
	model := nre.create_model("car.glb") or_return
	car := nre.create_instance(model) or_return
	...
}

main :: proc() {
	if err := run(); err != .Ok do fmt.eprintfln("%v: %s", err, nre.last_error_message())
}
```

- Errors: every wrapper returns an `Error_Code` last (`.Ok` = 0), so `or_return` works. Codes the bindings don't know become `.Unknown`. `last_error_message()` describes the last failure on this thread.
- Handles (`Model`, `Instance`, ...) are plain values. `{}` is the empty handle ("no texture", "no skybox"). Destroy them with `destroy_xxx`; nothing is freed for you.
- Math types are Odin builtins with the same memory layout as `nre.h`, so `core:math/linalg` works on them: `Vec2 :: [2]f32`, `Vec3 :: [3]f32`, `Quat :: quaternion128` (stored x, y, z, w; identity `Quat(1)`), `Mat4 :: matrix[4, 4]f32` (column-major like `NreMat4`, indexed `m[row, column]`). `Transform`'s zero value has scale 0, so use `nre.TRANSFORM_IDENTITY` or `nre.transform(position = {0, 0, -1})`.
- Optional results come back as `(value, ok, err)`: `hit, ok := nre.pick_instance(mouse) or_return`. `ok` is false when nothing is under the pixel.
- GLFW owns the window: call `set_render_config`, `init`, `poll_events`, the frame functions and input functions from the main thread.

## New in 0.3

- **Breaking:** `create_texture(path, srgb := true)`. The C function takes `srgb` now. Existing Odin callers keep working; pass `srgb = false` for data maps (normal and metallic-roughness).
- `create_mesh_model(vertices: []Vertex, indices: []u32, material := Material{})`: a model from your own triangles. The slices go to C as they are, with no copy (the library copies them). `Vertex` is `{position: Vec3, normal: Vec3, uv: Vec2}`; a zero normal is computed from the triangles.
- `set_casts_shadows(instance, casts_shadows)`.
- Scene lighting on the bound scene: `set_scene_lighting(.Small | .Landscape)`, `get_scene_lighting()` (values the bindings don't know become `.Small`), `set_shadow_distance(meters)`, `set_haze(visibility, height := 300)`, `set_exposure(exposure)`, `set_sky_color(color)`.
- `get_projection_matrix()` (Vulkan conventions: +Y down, reverse-Z, infinite far plane).
- `pick_instance(window_pos) -> (hit: Ray_Hit, ok: bool, err: Error_Code)`: `Ray_Hit` is `{instance, distance, point}`.
- `Error_Code.Invalid_Argument` (30), e.g. a negative shadow distance or picking in `Render_2D`.

## Requirements

- Odin (tested with dev-2026-10).
- A built `libnre_c` (NRE 0.3), e.g. a Debug build: `cmake-build-debug/bin/libnre_c.so`. At startup the bindings check that the library's version is 0.3.x and exit with a message if it isn't.

## Pointing it at libnre_c

Odin can't read environment variables in a `foreign import`, so the package links `system:nre_c` (`-lnre_c`) and the directory goes to the linker:

```sh
odin build myapp -extra-linker-flags:"-L$NRE_LIB_DIR -Wl,-rpath,$NRE_LIB_DIR"
```

The program then loads `libnre_c.so.0.3` through that rpath. Without the `-rpath` part, the system's normal search finds it (an installed library or `LD_LIBRARY_PATH`). `-define:NRE_C_LIB=/path/to/libnre_c.so` doesn't work: the Odin compiler rejects a file path there ("Invalid import path"), so pass the directory with `-L` and `-rpath` as above.

`run.sh` passes these flags for you. `NRE_LIB_DIR` defaults to `<repo>/cmake-build-debug/bin`:

```sh
./run.sh test     # headless tests (no window): version, error strings, error path, struct layouts, enum values
./run.sh orbit    # the orbit example: police car, plains skybox, drag to orbit, scroll to zoom
./run.sh build    # only builds the example, to build/orbit
```

The example (`examples/orbit`, a port of `example/main.cpp`) loads assets from `<repo>/example/assets` and shaders from `$NRE_SHADER_DIR`, or by default `<repo>/cmake-build-debug/example/shaders`. Change `API_MODE` in it to switch between `.Function_Draw` and `.Scene`.

Calling into the renderer before `set_render_config` aborts the process (no config yet means the error policy is Panic), so the tests call `set_render_config` first and never call `init`.
