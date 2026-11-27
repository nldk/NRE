# NRE for Swift

Swift bindings for NRE's C API (`include/nre.h`, ABI 0.3). Every nre.h function is wrapped as a static
function on `NRE` (`nreCreateModel` -> `NRE.createModel`); see `include/renderer.h` for what each one does.

```swift
import Nre

try NRE.setRenderConfig(width: 800, height: 600, title: "demo")
try NRE.initialize()                       // nreInit; `init` is reserved in Swift
let car = try NRE.createInstance(try NRE.createModel("police.glb"))
```

## Requirements

- Swift 6 (tested with 6.4.3 on Linux), SwiftPM. Builds in Swift 6 language mode with no warnings.
- A built `libnre_c` (e.g. `cmake-build-debug/bin/libnre_c.so`, which finds `librenderer` next to itself).

## Pointing it at libnre_c

The `CNre` module (`Sources/CNre/module.modulemap`) includes `../../include/nre.h` through a shim header and
links `-lnre_c`. The library is bound at link time, so the linker needs to find it:

- **`NRE_LIB_DIR`** (simplest): `Package.swift` adds `-L $NRE_LIB_DIR` and an rpath to it.
  SwiftPM re-evaluates the manifest when the variable changes.
  ```sh
  export NRE_LIB_DIR=/path/to/VulkanRenderer/cmake-build-debug/bin
  swift build
  ```
- **Linker flags**, e.g. when this package is a dependency (SwiftPM rejects the manifest's unsafeFlags there):
  `swift build -Xlinker -L$DIR -Xlinker -rpath -Xlinker $DIR`
- **Neither**: the system's linker and loader paths (an installed libnre_c, or `LD_LIBRARY_PATH` at run time).

At run time the loader takes `libnre_c.so.0.3` (its SONAME; `libnre_c.0.3.dylib` on macOS). The first NRE call
also checks `nreGetVersion` and throws `NreError(code: .libraryLoadFailed)` if the library isn't 0.3.x;
`NRE.checkVersion()` does that up front.

The `CNre` module reads `include/nre.h` directly, so after the header changes do a clean build (`rm -rf .build`)
to make sure the module cache picks it up.

## Run the example and tests

```sh
export NRE_LIB_DIR=/path/to/VulkanRenderer/cmake-build-debug/bin
swift run OrbitExample   # the C++ example's police car + plains skybox, with an orbit camera
swift test               # headless checks, no window
```

The example finds the repo from its own source path: assets from `example/assets`, shaders from
`$NRE_SHADER_DIR` (default `cmake-build-debug/example/shaders`). Drag with the left mouse button to orbit,
scroll to zoom. Switch `apiMode` at the top of `Sources/OrbitExample/OrbitExample.swift` between
`.functionDrawMode` and `.sceneMode`.

## The API

- **Main actor.** `NRE` is `@MainActor`: GLFW must be used from the main thread, and nre.h's error messages are
  per thread. Call it from `@main` / top-level code or other main-actor code. `NRE.version`,
  `NRE.checkVersion()` and `NRE.errorCodeToString` are nonisolated.
- **Errors.** Every call `throws(NreError)` (typed throws). `NreError` has `code: ErrorCode` and `message`
  (`nreGetLastErrorMessage()` read right after the failed call). `ErrorCode(code:)` maps values these bindings
  don't know to `.unknown`. Closures that call NRE need `{ () throws(NreError) in ... }` to keep the typed error;
  otherwise Swift infers `any Error`.
  The error policy matters: under `.ignore` most failures print and still return success, so `try` sees nothing.
  `setRenderConfig` defaults to `.return`.
- **Handles** (`Model`, `Instance`, `Texture`, ...) are small `Hashable` values. Nothing destroys them for you:
  call `NRE.destroyXxx`. `.empty` is the empty handle. Parameters where nre.h takes the empty handle
  (`createMaterial`'s textures, `setMaterialBaseTexture`, `setSkybox`) are Optionals, and `nil` means none.
- **Math.** `Vec2`, `Vec3` (`+ - * /`, `SIMD3<Float>` conversions via `Vec3(simd)` / `.simd`), `Quat`
  (x, y, z, w; `.identity`), `Mat4` (16 floats, column-major, `m[column, row]`), `Transform`
  (`Transform(position:rotation:scale:)`, all defaulted; `.identity`). Build them as `Vec3(0, 1, 0)`.
- **Defaults** follow renderer.h: `setRenderConfig(width:height:title:mode: .render3D, apiMode: .sceneMode,
  errorPolicy: .return, manualFramePresentationAndSubmission: false, resizable: true, showFpsInTitle: true)`,
  lights and `setSun` default to white with intensity 3 (directional / sun) or 10 (point), and
  `drawModel(model)` uses the identity transform.
- `getModelMaterials` returns `[Material]` (it does nre.h's two-call enumeration itself).
- **Own geometry.** `createMeshModel(vertices: [Vertex], indices: [UInt32], material: Material? = nil)`: a triangle
  list, counter-clockwise front faces. `Vertex(position:normal:uv:)` defaults `normal` and `uv` to zero, and a zero
  normal is computed from the triangles. `Vertex` has `NreVertex`'s layout (checked), so the array goes to C
  without a copy.
- **Scene lighting** (on the bound scene): `setSceneLighting(.small / .landscape)` / `getSceneLighting()`,
  `setShadowDistance(_:)` (meters; negative throws `.invalidArgument`), `setHaze(visibility:height: = 300)`,
  `setExposure(_:)`, `setSkyColor(_:)`, and per instance `setCastsShadows(_:_:)`.
- **Picking.** `pickInstance(at: Vec2) -> RayHit?`: the nearest visible instance under a pixel
  (`getMousePosition` units), with `instance`, `distance` and world-space `point`; nil when nothing is hit.
  `getProjectionMatrix()` returns the Vulkan projection (clip-space +Y down, reverse-Z, infinite far plane).

## Changes in 0.3

- **Breaking in C, not in Swift:** `nreCreateTexture` takes `srgb`. `createTexture(_ path:, srgb: Bool = true)`
  keeps the old behavior for existing calls; pass `srgb: false` for normal and metallic-roughness maps.
- New: `createMeshModel`, `setCastsShadows`, `setSceneLighting` / `getSceneLighting`, `setShadowDistance`,
  `setHaze`, `setExposure`, `setSkyColor`, `getProjectionMatrix`, `pickInstance`; the types `Vertex`, `RayHit`,
  `SceneLighting`; `ErrorCode.invalidArgument` (30).
- The version check now accepts only libnre_c 0.3.x.

## Layout

```
Package.swift
Sources/CNre/            module.modulemap + shim.h: the C API as a Clang module
Sources/Nre/             the Swift API (NRE.swift, Error.swift, Enums.swift, Math.swift, Handles.swift)
Sources/OrbitExample/    port of example/main.cpp
Tests/NreTests/          headless swift-testing checks
```
