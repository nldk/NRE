# NRE for Zig

Zig bindings for NRE's C API (`include/nre.h`, `libnre_c`). Module `nre` (`src/nre.zig`): the C++
functions in Zig style (`NRE::createModel` -> `nre.createModel`), error unions, handle and math types.
The raw translated header is `nre.c`.

## Requirements

- Zig 0.17.1
- A built `libnre_c` 0.3.x (e.g. `cmake-build-debug/bin/libnre_c.so`, SONAME `libnre_c.so.0.3`)
- For the example: the compiled demo shaders and `example/assets`

## Pointing it at libnre_c

`build.zig` links `nre_c`. The library directory comes from `-Dnre-lib-dir=<dir>`, else the env var
`NRE_LIB_DIR`, else the linker's normal search paths. The example and the tests get that directory as an
rpath, so they run without `LD_LIBRARY_PATH`. At startup (`setRenderConfig*`, `init`) the bindings check
that the library is 0.3.x and return `error.VersionMismatch` otherwise.

```sh
export NRE_LIB_DIR=/path/to/VulkanRenderer/cmake-build-debug/bin
zig build test   # headless checks, no window
zig build run    # examples/orbit.zig: police car, plains skybox, orbit camera (drag + scroll)
```

The example finds assets in `<repo>/example/assets` (the repo path is baked in at build time) and shaders
in `$NRE_SHADER_DIR`, default `<repo>/cmake-build-debug/example/shaders`.

## Using it from another package

```zig
// build.zig.zon: .nre = .{ .path = "../VulkanRenderer/bindings/zig" },
const nre = b.dependency("nre", .{ .target = target, .optimize = optimize, .@"nre-lib-dir" = lib_dir });
exe.root_module.addImport("nre", nre.module("nre"));
exe.root_module.addRPath(b.graph.cwdRelativePath(lib_dir));
```

The header is read from `../../include/nre.h`, so the package must stay inside the NRE repo.

## Errors

Zig errors carry no payload, so the details sit next to them: right after a call fails,
`nre.lastErrorMessage()` is the renderer's message and `nre.lastErrorCode()` the raw `ErrorCode`
(codes newer than these bindings become `error.Unknown`, their raw value is still in `lastErrorCode()`).

```zig
const model = nre.createModel("car.glb") catch |err| {
    std.log.err("{t}: {s}", .{ err, nre.lastErrorMessage() });
    return err;
};
```

## Defaults

No config structs. `setRenderConfig(width, height, title)` uses the defaults (3D, SceneMode, error policy
Return, auto present/submit, resizable, FPS in title); `setRenderConfigFull(...)` takes every setting.
Light/sun `color` and `intensity` are optionals: `null` means `{1,1,1}` and 3 (directional, sun) or 10 (point).
`drawModel(model, null)` draws with the identity transform.

`createTexture(path)` loads a colour (sRGB) texture; `createTextureFull(path, srgb)` takes `srgb = false`
for data maps (normal and metallic-roughness maps). `setHaze(visibility, null)` uses the default height of
300 m.

## New in 0.3

- `createMeshModel(vertices: []const Vertex, indices: []const u32, material: ?Material)`: a model from your
  own triangles. The slices go to C as-is (no copy in the bindings); a zero `normal` is computed from the
  triangles; `null` material is the default one.
- `setSceneLighting(.small / .landscape)`, `getSceneLighting()`. `SceneLighting` is non-exhaustive, so a
  mode newer than these bindings comes back as a `_` value.
- `setShadowDistance(meters)`, `setHaze(visibility, ?height)`, `setExposure(e)`, `setSkyColor(color)`,
  `setCastsShadows(instance, casts)`.
- `getProjectionMatrix()` (Vulkan conventions: +Y down, reverse-Z, infinite far plane).
- `pickInstance(window_pos) Error!?RayHit`: `null` when nothing is under the pixel.
- `error.InvalidArgument` (e.g. a negative shadow distance, or picking in Render_2D).
- Breaking in the C API: `nreCreateTexture` takes `srgb`. `nre.createTexture(path)` keeps its signature
  (srgb true, as before).

Call NRE from the main thread: GLFW needs it for the window and input functions.
