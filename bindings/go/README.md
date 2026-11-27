# NRE Go bindings

Go bindings (cgo) for NRE's C API (`include/nre.h`, `libnre_c`). Package `nre`, import path
`github.com/nldk/NRE/bindings/go/nre`. The functions mirror the C++ API in `include/renderer.h`
(`NRE::createModel` is `nre.CreateModel`); see renderer.h for what each one does.

## Requirements

- Go 1.22+ and a C compiler (cgo).
- libnre_c 0.3.x, built from this repo (e.g. the Debug build puts it in `cmake-build-debug/bin`, next to
  librenderer). The bindings check the version in `SetRenderConfig`/`Init` and return `ErrLibraryLoadFailed`
  if it isn't 0.3.x.

## Pointing it at libnre_c

The package links `-lnre_c`; headers come from `include/` automatically. cgo directives can't read
environment variables, so the library directory (`NRE_LIB_DIR` in the other bindings) goes in through
`CGO_LDFLAGS`. The rpath lets the binary find it at run time without `LD_LIBRARY_PATH`:

```sh
export NRE_LIB_DIR=/path/to/VulkanRenderer/cmake-build-debug/bin
export CGO_LDFLAGS="-L$NRE_LIB_DIR -Wl,-rpath,$NRE_LIB_DIR"
go build ./...
```

Without the rpath, the loader looks for `libnre_c.so.0.3` the usual way (`LD_LIBRARY_PATH`, system dirs).
The `Makefile` does the export for you, with `NRE_LIB_DIR` defaulting to `../../cmake-build-debug/bin`:

```sh
make test                  # headless tests, no window
make vet
make run                   # builds build/orbit and runs it
NRE_LIB_DIR=/other/dir make run
```

## Usage

```go
package main

import (
	"errors"
	"log"
	"runtime"

	"github.com/nldk/NRE/bindings/go/nre"
)

// GLFW must run on the main OS thread: lock main's goroutine to it, and call nre from main only.
func init() { runtime.LockOSThread() }

func main() {
	if err := nre.SetRenderConfig(800, 600, "hello"); err != nil { log.Fatal(err) }
	if err := nre.Init(); err != nil { log.Fatal(err) }
	defer nre.Shutdown()

	model, err := nre.CreateModel("box.glb")
	if errors.Is(err, nre.ErrFileNotFound) { log.Fatal("no box.glb: ", err) }
	// ...
}
```

- **Errors**: every failing call returns an `*nre.Error{Code, Message}`. Each `ErrorCode` constant is
  itself an error, so `errors.Is(err, nre.ErrInvalidHandle)` and `errors.As(err, &code)` work. Codes from a
  newer library that these bindings don't know become `ErrUnknown`.
- **Handles** (`Model`, `Instance`, `Texture`, ...) are comparable values; the zero value is the empty
  handle (`nre.Texture{}` = no texture, `nre.Cubemap{}` = no skybox). Nothing is freed automatically: call
  the `DestroyXxx` functions like in C++.
- **Defaults**: Go has no default arguments, so functions with defaults have a short form and a `Full`
  form: `SetRenderConfig(w, h, title)` / `SetRenderConfigFull(w, h, title, mode, apiMode, errorPolicy,
  manual, resizable, showFps)`, `CreateDirectionalLight(dir)` / `...Full(dir, color, intensity)`,
  likewise `CreatePointLight`, `SetSun`, `CreateMaterial`, `CreateTexture(path)` (sRGB) /
  `CreateTextureFull(path, srgb)`, `SetHaze(visibility)` (300 m layer) / `SetHazeFull(visibility, height)`,
  and `DrawModel(model)` (identity transform) / `DrawModelFull(model, transform)`.
- **Own geometry**: `CreateMeshModel(vertices []nre.Vertex, indices []uint32, material)` (`nre.Material{}`
  = default material). `Vertex` has `NreVertex`'s layout (checked at compile time), so the slices go to C
  without a copy; a zero `Normal` is computed from the triangles.
- **Picking**: `hit, ok, err := nre.PickInstance(pos)` (`pos` in `GetMousePosition` units); `ok` is false
  when nothing is under the pixel. `RayHit` has `Instance`, `Distance` and `Point`.
- **Lighting** (bound scene): `SetSceneLighting(nre.SceneLightingSmall | nre.SceneLightingLandscape)` /
  `GetSceneLighting`, `SetShadowDistance`, `SetHaze`, `SetExposure`, `SetSkyColor`, and
  `SetCastsShadows(instance, bool)` per instance. Invalid values (e.g. a negative shadow distance) give
  `ErrInvalidArgument`. `GetProjectionMatrix` returns the Vulkan projection (+Y down, reverse-Z).
- **Math**: `Vec2`, `Vec3` (with `Add`/`Sub`/`Scale`), `Quat` (X, Y, Z, W; `QuatIdentity()`), `Mat4`
  (`[16]float32`, column-major), `Transform` (`IdentityTransform()`; the zero value has scale 0).
- **0.2 → 0.3**: `CreateTexture(path)` keeps working (sRGB, as before); use `CreateTextureFull(path, false)`
  for normal and metallic-roughness maps. Everything else is new: the functions above and `ErrInvalidArgument`.
- Before `SetRenderConfig` the renderer has no error policy and aborts the process on any error (Debug
  builds print `NotInitialized` first), so configure it first.

## Example

`examples/orbit` is example/main.cpp in Go: the police car on the plains skybox with an orbit camera (drag
with the left mouse button, scroll to zoom). Assets come from `<repo>/example/assets`, shaders from
`$NRE_SHADER_DIR` (default `<repo>/cmake-build-debug/example/shaders`); `<repo>` is found from the source
file's location, so the working directory doesn't matter.

```sh
make run
# or: CGO_LDFLAGS=... go run ./examples/orbit
```
