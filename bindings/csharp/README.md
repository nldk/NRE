# NRE for C# / .NET

C# bindings over NRE's C API (`include/nre.h`, `libnre_c`). They cover all of nre.h; the functions are
renderer.h's, in C# style (`NRE.CreateModel`, `NRE.SetCameraPosition`, ...). See renderer.h for what each one does.

```csharp
using Nre;

NRE.SetRenderConfig(800, 600, "hello");          // mode, apiMode, errorPolicy, ... have defaults
NRE.Init();
var car = NRE.CreateInstance(NRE.CreateModel("police.glb"));
NRE.TranslateInstance(car, (0f, 0f, -1f));       // Vec3 from a tuple, System.Numerics.Vector3 or new Vec3(...)
while (!NRE.WindowShouldClose()) { NRE.PollEvents(); NRE.RenderFrame(); }
NRE.Shutdown();
```

- **Errors** throw `NreException`. It has the `ErrorCode Code` and the library's message (`NativeMessage`, read
  with nreGetLastErrorMessage right after the failed call). Codes these bindings don't know become `ErrorCode.Unknown`.
  Exceptions only come back when the error policy returns errors. `RenderErrorPolicy.Return` is the default here.
  `Ignore` makes the renderer carry on after a failure, and `Panic` aborts the process.
- **Handles** (`Model`, `Instance`, `Texture`, ...) are 8-byte values (`readonly record struct`). `X.None`, or
  `default`, is the empty handle. Nothing is destroyed for you: call `NRE.DestroyXxx` like in C++. Parameters
  that take "no texture" or "no skybox" are nullable (`Texture?`, `Cubemap?`).
- **Math**: `Vec2`, `Vec3`, `Quat` (x, y, z, w), `Transform` (`Transform.Identity with { Position = ... }`) and
  `Mat4` (column-major). They convert to and from System.Numerics.
- **Your own geometry**: `NRE.CreateMeshModel(ReadOnlySpan<Vertex> vertices, ReadOnlySpan<uint> indices,
  Material? material = null)`. `Vertex(Position, Normal = default, Uv = default)` has NreVertex's layout, so arrays,
  `List<Vertex>` (via `CollectionsMarshal.AsSpan`) and stackalloc spans are passed to the library as they are, with
  no copy in the bindings. A zero normal is computed from the triangles.
- **Picking**: `NRE.PickInstance(Vec2 windowPos)` returns a `RayHit?` (`Instance`, `Distance`, `Point`), null when
  nothing is under the pixel.
- **Lighting**: `NRE.SetSceneLighting(SceneLighting.Small | Landscape)` / `GetSceneLighting()`, `SetShadowDistance`,
  `SetHaze(visibility, height = 300)`, `SetExposure`, `SetSkyColor` and `SetCastsShadows(instance, bool)`, all on the
  bound scene. `GetProjectionMatrix()` returns the Vulkan projection (+Y down, reverse-Z, infinite far plane).
- **Threading**: GLFW requires the main thread, so call NRE from the thread that called `Init`.

## Requirements

- .NET SDK 10 (net10.0).
- A built `libnre_c`, for example the Debug build in `<repo>/cmake-build-debug/bin`. `libnre_c.so.0.3` finds
  `librenderer` next to itself.
- To run the example: a Vulkan GPU and the compiled demo shaders (`cmake-build-debug/example/shaders/*.spv`).

## Pointing it at libnre_c

- With `NRE_LIB_DIR=/path/to/dir` set, the library is loaded from that directory.
- Without it, `libnre_c.so.0.3` (Linux), `libnre_c.0.3.dylib` (macOS) or `nre_c.dll` (Windows) is loaded through
  the normal search: the app directory, `LD_LIBRARY_PATH`, and so on.

After loading, the library's version is checked. These bindings target 0.3.x. Another major.minor version
throws `NreException` (`LibraryLoadFailed`) on the first call.

The example and the tests fall back to `<repo>/cmake-build-debug/bin` when `NRE_LIB_DIR` isn't set.

## Example and tests

```sh
cd bindings/csharp
NRE_LIB_DIR=$PWD/../../cmake-build-debug/bin dotnet run --project Nre.Examples.Orbit
dotnet test
```

The example is a port of `example/main.cpp`: an orbit camera (drag with the left button, scroll to zoom) around
the police car, with the plains skybox. Set `apiMode` at the top of `Nre.Examples.Orbit/Program.cs` to switch
between SceneMode and FunctionDrawMode. It loads assets from `<repo>/example/assets` and shaders from
`NRE_SHADER_DIR`, or from `<repo>/cmake-build-debug/example/shaders` when that isn't set.

## Changes from 0.2

- `CreateTexture(string path, bool srgb = true)`: new `srgb` parameter. Pass `srgb: false` for data maps (normal
  and metallic-roughness maps). Existing calls keep loading color (sRGB) textures. nreCreateTexture itself gained
  the parameter, so these bindings need libnre_c 0.3.
- New: `CreateMeshModel`, `Vertex`, `PickInstance`, `RayHit`, `SceneLighting`, `SetSceneLighting`,
  `GetSceneLighting`, `SetShadowDistance`, `SetHaze`, `SetExposure`, `SetSkyColor`, `SetCastsShadows`,
  `GetProjectionMatrix`, and `ErrorCode.InvalidArgument` (e.g. a negative shadow distance).

The tests don't need a window. They cover the version check, error code names and mapping, the errors
returned after `SetRenderConfig` (`AlreadyInitialized`, `InvalidHandle`), struct sizes and field order against
nre.h (including `Vertex` and `RayHit`), enum values parsed from nre.h, and that `Native` declares exactly the
functions nre.h exports. They also check that a library of another version (the stale 0.2 build, if it's still in
the build directory) is rejected. A call made before `SetRenderConfig` aborts the process in the
renderer, so that case runs in a child process.
