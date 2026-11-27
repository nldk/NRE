# NRE for Lua (LuaJIT FFI)

`nre.lua` is a single pure-Lua module over NRE's C API (`include/nre.h`, `libnre_c`). It needs **LuaJIT 2.1**
because it uses LuaJIT's `ffi`. Plain PUC Lua (5.1 to 5.5) has no FFI, so it would need a C module. That module
isn't included, and `require("nre")` under PUC Lua fails with a message that says so.

```lua
local nre = require("nre")
nre.set_render_config(800, 600, "hello")
nre.init()
local model = nre.create_model("box.glb")
```

## Pointing it at libnre_c

- If `NRE_LIB_DIR` is set, the module loads `$NRE_LIB_DIR/libnre_c.so.0.3` (`libnre_c.0.3.dylib` on macOS,
  `nre_c.dll` on Windows). It finds `librenderer` next to itself.
- Otherwise it loads the library by that name through the normal search (`LD_LIBRARY_PATH`, the system dirs).

After loading, it checks `nreGetVersion` and errors out if the library isn't 0.3.x, the ABI these bindings target.

With a Debug build in this repo:

```sh
export NRE_LIB_DIR=/path/to/VulkanRenderer/cmake-build-debug/bin
```

Put `bindings/lua` on `package.path` (`package.path = "/path/to/bindings/lua/?.lua;" .. package.path`), or copy
`nre.lua` next to your script.

## Running the example and the tests

From `bindings/lua` (they set `package.path` from their own location, so any CWD works):

```sh
luajit examples/orbit.lua     # police car, plains skybox, orbit camera: drag with the left button, scroll to zoom
luajit tests/headless.lua     # no window; exits non-zero on failure
```

The headless tests also check `nre.lua` against `include/nre.h` when they run inside the repo: the CDEF block
must declare exactly nre.h's functions, enum constants and structs, and every function needs a snake_case wrapper.

The example takes shaders from `$NRE_SHADER_DIR`, which defaults to `<repo>/cmake-build-debug/example/shaders`,
and assets from `<repo>/example/assets`. Edit `API_MODE` at the top of the example to switch between
`nre.ApiMode.FUNCTION_DRAW` and `nre.ApiMode.SCENE`.

## The API

Functions mirror `renderer.h` in snake_case (`nreCreateModel` → `nre.create_model`). See `renderer.h` for what
each one does. Every function in `nre.h` is wrapped, and `nre.lib` is the raw C namespace if you need it.
Run GLFW-related calls (`init`, `poll_events`, `window_should_close`, input) on the main thread. Coroutines are
fine; other OS threads (lanes, effil) are not.

**Errors are raised, not returned.** A failed call raises a table
`{ code = <number>, name = "FileNotFound", message = "<nreGetLastErrorMessage()>" }`. It has a `__tostring`,
so an uncaught error prints readably. `code` is the raw value, and an unknown code gets `name = "Unknown"`.

```lua
local ok, err = pcall(nre.create_model, "missing.glb")
if not ok then
    if nre.is_error(err) and err.code == nre.ErrorCode.FILE_NOT_FOUND then print(err.message) else error(err, 0) end
end
-- for a traceback: xpcall(main, function(e) return debug.traceback(tostring(e)) end)
```

Why raising instead of the `nil, message` convention: nearly every NRE call returns nothing or a value that is
always used, so `nil, msg` would mean checking every call. In the example's frame loop, forgetting a check would
silently go on with a `nil` handle. Raising matches the C++ `.value()` style, keeps the happy path short, and
`pcall` gives you the `nil, msg` behaviour where you want it. The error table's position info comes from the
traceback, not the message (Lua only adds `file:line` to string errors).

**Defaults** use plain parameters, with `nil` meaning "default" (no options tables):
`set_render_config(width, height, title, mode=RENDER_3D, api_mode=SCENE, error_policy=RETURN,
manual_frame_presentation_and_submission=false, resizable=true, show_fps_in_title=true)`, then
`create_directional_light(dir, color={1,1,1}, intensity=3)`, `set_sun(...)` (same defaults),
`create_point_light(pos, color={1,1,1}, intensity=10)`, `draw_model(model, transform=identity)`,
`create_texture(path, srgb=true)`, `create_mesh_model(vertices, indices, material=default)` and
`set_haze(visibility, height=300)`.

**Enums** are tables of constants without the C prefix, for example `nre.Key.W`, `nre.Key["0"]`, `nre.Key.KP_0`,
`nre.ShaderType.VERTEX`, `nre.ApiMode.FUNCTION_DRAW`, `nre.RenderMode.RENDER_3D`, `nre.SceneLighting.LANDSCAPE`
and `nre.ErrorCode.INVALID_HANDLE`. `get_scene_lighting()` returns a plain number to compare with
`nre.SceneLighting.*`; a value newer than these bindings comes back as that number. The tables are generated from the cdef block. Enum arguments also accept the
short name as a string (`"W"`) or the C name (`"NRE_KEY_W"`, which LuaJIT converts itself).

**Math**: `nre.vec2/vec3/quat/transform(...)` build values from numbers, `{x, y, z}`, `{x=, y=, z=}` or another
value. `nre.quat()` is the identity, and `nre.transform()` is identity with any part optional (also
`nre.transform{position = {0, 1, 0}}`). Functions taking vectors accept the same table forms. Returned values are
cdata (`nre.Vec3` etc.). Vec2/Vec3 support `+ - * /`, unary `-`, `==`, `tostring`, `:dot`, `:length`, and on
Vec3 also `:cross` and `:normalized`. `Mat4` is column-major (`m.m[col * 4 + row]` or `m:at(col, row)`).
One LuaJIT detail to watch: a struct field such as `transform.position` is a *reference* into its parent. Copy
it with `nre.vec3(t.position)` before keeping it longer than `t`.

**Handles** (`nre.Model`, `nre.Texture`, ...) are 8-byte cdata values you copy freely. They compare with `==`,
and `h:is_empty()` tests for the empty handle. Calling the type, as in `nre.Texture()`, gives the empty handle.
`nil` also counts as empty where the C API gives `{0}` a meaning: `create_material`'s textures,
`set_material_base_texture` and `set_skybox(nil)` (which removes the skybox). There are no finalizers, so
destroy GPU objects yourself, as in C++.

`get_model_materials(model)` returns a 1-based Lua list. To match it, `replace_model_material(model, index,
material)` takes a **1-based** index (the C index + 1).

**Models from your own geometry**: `create_mesh_model(vertices, indices, material)` (counter-clockwise front
faces, three indices per triangle; see `createMeshModel` in renderer.h). Two input forms, mixable:

- Lua lists. Each vertex is anything `nre.vertex` accepts: `{position = {x,y,z}, normal = {x,y,z}, uv = {u,v}}`,
  `{{x,y,z}, {x,y,z}, {u,v}}`, a bare position `{x,y,z}` or an `nre.Vertex`. Missing parts are zero, and a zero
  normal is computed from the triangles. Indices in a Lua list are **1-based** like every Lua list here
  (`vertices[1]` is index 1), and the bindings subtract 1. Index 0 or a non-integer raises `INVALID_ARGUMENT`.
- cdata arrays, passed to C without a copy (fast for big procedural meshes): `ffi.new("NreVertex[?]", n)` for
  vertices and `ffi.new("uint32_t[?]", n)` for indices. Those indices are **0-based**, raw C values. Pass the
  array itself, not a pointer, because only an array carries its length.

```lua
local tri = nre.create_mesh_model({ {0, 0, 0}, {1, 0, 0}, {0, 1, 0} }, { 1, 2, 3 })
```

**Picking**: `pick_instance(window_pos)` returns a new `nre.RayHit` (`hit.instance`, `hit.distance`,
`hit.point`) or `nil` when nothing is under the pixel. `window_pos` is in `get_mouse_position()` units.

**New in 0.3**: `create_mesh_model`, `nre.vertex`, `set_casts_shadows`, `set_scene_lighting` /
`get_scene_lighting`, `set_shadow_distance` (negative raises `INVALID_ARGUMENT`), `set_haze`, `set_exposure`,
`set_sky_color`, `get_projection_matrix` (Vulkan conventions: +Y down, reverse-Z, infinite far plane),
`pick_instance`, and the error code `nre.ErrorCode.INVALID_ARGUMENT` (30). `create_texture` gained `srgb`
(default `true`; pass `false` for normal and metallic-roughness maps). Existing `create_texture(path)` calls
keep working.

## Keeping it in sync with nre.h

The `CDEF` string at the top of `nre.lua` is `nre.h` with the preprocessor parts written out (handles
expanded, `NRE_C_EXPORT` and `extern "C"` dropped), because `ffi.cdef` has no preprocessor. When `nre.h`
changes, update that block, then the wrapper below it. The enum tables update themselves.
