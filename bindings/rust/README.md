# NRE Rust bindings

Rust bindings for the NRE renderer over its C API (`include/nre.h`, libnre_c), in a Cargo workspace:

- `nre-sys`: the raw `extern "C"` declarations, hand-written from nre.h (no bindgen). C enums are `i32` aliases
  plus constants. Links `nre_c` as a dylib.
- `nre`: the safe API, in two layers:
  - Free functions named like the C++ ones (`NRE::createModel` is `nre::create_model`), each returning
    `nre::Result<T>`. See renderer.h for what each function does.
  - A typed layer on top of them, `nre::Renderer<M>`, that turns misuse into compile errors. See
    [The typed layer](#the-typed-layer).

## Requirements

- Rust 1.88 or newer (edition 2024).
- A build of libnre_c 0.3.x (`libnre_c.so.0.3` with `librenderer.so.0.3` next to it), e.g. from the repo's
  CMake build: `cmake-build-debug/bin`.

## Pointing it at libnre_c

Set `NRE_LIB_DIR` to the directory that holds libnre_c, as an **absolute** path, when you build:

```sh
export NRE_LIB_DIR=/path/to/VulkanRenderer/cmake-build-debug/bin
```

- At link time, `nre-sys` adds it to the library search path. Without it, the linker's normal search finds
  `libnre_c.so` (Linux), `libnre_c.dylib` (macOS) or `nre_c.lib` (Windows).
- At run time, the `nre` crate's own examples and tests get it as an rpath, so they run without
  `LD_LIBRARY_PATH`. **Your own binaries don't get this.** Install the library where the loader looks, add an
  rpath yourself (`cargo::rustc-link-arg=-Wl,-rpath,<dir>` in your build.rs), or set
  `LD_LIBRARY_PATH` / `DYLD_LIBRARY_PATH` / `PATH` (Windows has no rpath: put `nre_c.dll` next to the exe or on `PATH`).
- Before its first call into the library, every wrapper checks once that the loaded libnre_c
  is version 0.3.x (`nre::check_version()`, cached). On a mismatch (an old 0.2 included), every call fails with
  `ErrorCode::LibraryLoadFailed`.

## Changes in 0.3

For libnre_c 0.3 (renderer v0.3.0). New:

- Scene lighting on the bound scene: `set_scene_lighting(SceneLighting::Landscape)` / `get_scene_lighting()`,
  `set_shadow_distance(meters)`, `set_haze` / `set_haze_with`, `set_exposure`, `set_sky_color`, and per instance
  `set_casts_shadows(instance, bool)`.
- `create_mesh_model(&[Vertex], &[u32], Option<Material>)`: models from your own geometry.
- `get_projection_matrix()` and `pick_instance(window_pos) -> Option<RayHit>`.
- `ErrorCode::InvalidArgument` (e.g. a negative shadow distance, or picking in Render_2D).
- New types: `SceneLighting`, `Vertex`, `RayHit` (all in the prelude).

**Breaking in the C API:** `nreCreateTexture` takes `srgb`. In Rust, `create_texture(path)` keeps working and
loads as sRGB (renderer.h's default) for colour images. Load normal and metallic-roughness maps with
`create_texture_with(path, false)`. Code calling `nre::sys::nreCreateTexture` directly needs the new argument.

Every new function is also a `Context` method (all `&self`) for the typed layer.

## Usage notes

- GLFW needs the main thread. Call `init`, `poll_events`, the frame functions and the input functions from `main`'s thread.
- Handles (`Model`, `Instance`, ...) are `Copy` values. Free them with `destroy_*`, like in C++. Dropping one does
  nothing. `Handle::NONE` is the empty handle. "No texture" and "no skybox" take `Option<Texture>` / `Option<Cubemap>`.
- Defaulted C++ parameters: the short function uses renderer.h's defaults, and the `_with` form takes all of them.
  Examples: `set_render_config(w, h, title)` and `set_render_config_with(...)`, or `create_point_light(pos)` and
  `create_point_light_with(pos, color, intensity)`, `create_texture(path)` (sRGB) and
  `create_texture_with(path, srgb)`, `set_haze(visibility)` (a 300 m layer) and `set_haze_with(visibility, height)`.
  `draw_model(model, None)` uses the identity transform.
- Vector parameters take `impl Into<Vec3>`, so `[0.0, 1.0, 0.0]`, `(0.0, 1.0, 0.0)` and `Vec3::Y` all work.
- `create_mesh_model(&vertices, &indices, material)` takes `&[Vertex]` and `&[u32]` and passes them to C as they
  are, without copying (`Vertex` is `#[repr(C)]` with `NreVertex`'s layout; the renderer copies the data).
  `material: None` uses the default material. A zero normal is computed from the triangles.
- `pick_instance(window_pos)` returns `Result<Option<RayHit>>`: `Ok(None)` when nothing is under the pixel.
- `get_scene_lighting()` turns a value from the library that these bindings don't know into
  `ErrorCode::Unknown`, never an invalid enum.
- A string or path with a NUL byte fails with `ErrorCode::Unknown` before reaching the library. So does a
  non-UTF-8 path on Windows.

## The typed layer

```rust
use nre::prelude::*;

fn main() -> nre::Result<()> {
    let mut nre = Renderer::<DrawMode>::new(500, 500, "test")?; // set_render_config + init
    let car = nre.create_instance(nre.create_model("police.glb")?)?;
    // ... create and bind a pipeline ...
    while !nre.window_should_close()? {
        nre.poll_events()?;
        let frame = nre.frame()?; // render_frame
        frame.draw_instance(car)?; // only exists in DrawMode
    } // frame dropped: end_frame
    Ok(())
} // nre dropped: shutdown
```

- **`Renderer<M>`**, `M` being `SceneMode` or `DrawMode` (types standing for `ApiMode::Scene` /
  `ApiMode::FunctionDraw`; the trait is `nre::Mode`). `Renderer::new(width, height, title)` uses renderer.h's
  defaults; `new_with(width, height, title, render_mode, error_policy, manual_frame_presentation_and_submission,
  resizable, show_fps_in_title)` takes the rest. Both call `set_render_config` and `init`. Dropping it calls
  `shutdown`; `nre.shutdown()` does the same and returns the error.
- **One at a time.** The C renderer is global, so while a `Renderer` exists, another `new` fails with
  `ErrorCode::AlreadyInitialized` without calling into the library. After it's dropped you can create a new one.
  If `init` itself fails, no other `Renderer` can be made in that process (the C side keeps its context, and
  shutting a half-initialized renderer down isn't safe). Don't mix in the low-level `set_render_config` / `init`
  / `shutdown`.
- **Main thread.** `Renderer` isn't `Send` or `Sync`, so it and everything borrowed from it stays on the thread
  that created it. Create it on `main`'s thread: GLFW wants that.
- **The API as methods.** Every free function except the setup, frame, draw, error and version ones is a method of
  `nre::Context`, which `Renderer` and `Frame` deref to: `nre.create_model(path)`, `nre.set_camera_position(p)`,
  and so on, with the same `_with` forms. Take `&Context` in your own helpers (like the orbit camera) so they
  work both outside and inside a frame.
- **`&self` vs `&mut self`.** All `Context` methods take `&self`: none of them can break a frame in progress.
  They change what gets drawn (scene, instances, camera, materials, lighting) or read input, and the renderer
  copes with that happening mid-frame (it reads the camera when the frame starts, an instance's transform when it's drawn,
  and skips destroyed models and scenes). What can't happen during a frame takes `&mut self` or `self`:
  `frame()` and `shutdown()`. A `Frame` borrows the renderer mutably, so you can't start a second frame, shut
  down, or drop the renderer while one is alive, and you still have the `&self` methods through the frame.
- **Frames.** `nre.frame()?` starts a frame (`render_frame`) and returns a `Frame<M>` guard. Dropping it ends
  the frame; `frame.end()?` does the same and returns the error. Only `Frame<DrawMode>` has `draw_instance` and
  `draw_model`. What ending does:

  | | automatic presentation (default) | `manual_frame_presentation_and_submission` |
  |---|---|---|
  | `SceneMode` | nothing: `render_frame` already drew, submitted and presented | `submit_frame`, `present_frame` |
  | `DrawMode` | `end_frame` | `end_frame`, `submit_frame`, `present_frame` |

  If `render_frame` fails in DrawMode, the frame is still open on the C side, so `frame()` closes it with
  `end_frame` before returning the error. Otherwise the next `frame()` would fail with `WrongMode`.
  Manual presentation is handled by the guard. For control over when to submit and present, use the low-level
  functions without a `Renderer`.
- **Owned handles (optional).** Handles stay `Copy` values. `nre.own(handle)` wraps one in an `Owned<'_, T>`
  that destroys it when dropped (`.destroy()` returns the error, `.release()` hands back the plain handle). It
  borrows the context, so it can't outlive the renderer. Borrowed from the `Renderer` itself, it would keep
  `frame()` from getting `&mut`. So for handles that live across frames, split the renderer first:

  ```rust
  let mut renderer = Renderer::<DrawMode>::new(800, 600, "owned")?;
  let (nre, mut frames) = renderer.split(); // &Context + the frame starter
  let model = nre.own(nre.create_model("police.glb")?);
  while !nre.window_should_close()? {
      nre.poll_events()?;
      let frame = frames.frame()?;
      frame.draw_model(*model, None)?;
  }
  ```
- **What doesn't compile** (each is a `compile_fail` doctest): a draw call on `Frame<SceneMode>`, two live
  frames at once, `shutdown()` during a frame, a frame or an `Owned` outliving its renderer, and sending the
  `Renderer` to another thread.
- **What the typed layer rules out at run time too:** calling anything before `set_render_config` (which
  aborts) and `create_model` between `set_render_config` and `init` (the VMA assert below). A `Renderer` is
  always configured and initialized.
- **`glam` and `mint` features** (off by default) add `From`/`Into` between nre's `Vec2`, `Vec3`, `Quat`,
  `Mat4` and glam's `Vec2`, `Vec3`, `Vec3A`, `Quat`, `Mat4` / mint's `Vector2`, `Point2`, `Vector3`, `Point3`,
  `Quaternion`, `ColumnMatrix4`. Vector parameters take `impl Into<Vec3>`, so `nre.set_camera_position(glam::Vec3::Y)`
  works, and returned values convert with `.into()`:

  ```toml
  nre = { path = "...", features = ["glam"] }
  ```

## Example

A port of `example/main.cpp`: the police car on the plains skybox with an orbit camera. Drag with the left
mouse button to orbit and scroll to zoom. Change `API_MODE` to switch between Scene and FunctionDraw mode.

```sh
cd bindings/rust
NRE_LIB_DIR=/path/to/VulkanRenderer/cmake-build-debug/bin cargo run --example orbit
```

`orbit_typed` is the same demo written with the typed layer (`Renderer` and the frame guard), in DrawMode like
main.cpp. Set `NRE_API_MODE=scene` to run it in SceneMode:

```sh
NRE_LIB_DIR=/path/to/VulkanRenderer/cmake-build-debug/bin cargo run --example orbit_typed
NRE_API_MODE=scene NRE_LIB_DIR=/path/to/VulkanRenderer/cmake-build-debug/bin cargo run --example orbit_typed
```

Assets come from `<repo>/example/assets`. Shaders come from `$NRE_SHADER_DIR`, which defaults to
`<repo>/cmake-build-debug/example/shaders`.

## Tests

```sh
NRE_LIB_DIR=/path/to/VulkanRenderer/cmake-build-debug/bin cargo test
NRE_LIB_DIR=/path/to/VulkanRenderer/cmake-build-debug/bin cargo test --features glam,mint
cargo clippy --all-targets --all-features
```

The tests are headless: no window, no GPU. They cover the version check (it must be 0.3), error-code names
(`InvalidArgument` included), enum conversions (`SceneLighting` values included), the error path (code and
message), NUL-byte handling, and that struct layouts match nre.h (`Vertex` and `RayHit` included). For the typed layer
they check the single-`Renderer` rule (without calling into the library) and the glam/mint conversions, and
`compile_fail` doctests check the misuse listed above. They run against a Debug build of the library, which
does the misuse checks.

`tests/window.rs` opens windows, so it's `#[ignore]`d. It needs a display and a Vulkan GPU. It runs frames in
both modes and with manual presentation, re-creates the renderer after a shutdown, recovers from a frame that
failed to start, uses owned handles across frames, and checks the 0.3 additions (Landscape lighting, the
shadow-distance `InvalidArgument` error, haze, exposure, sky colour, the projection matrix, picking a box and
missing it, a mesh model and a non-sRGB texture):

```sh
NRE_LIB_DIR=/path/to/VulkanRenderer/cmake-build-debug/bin cargo test --test window -- --ignored --test-threads=1
```

Calling an API function before `set_render_config` aborts the process, by design: until then the renderer
doesn't know which error policy you want, so it assumes Panic. One test checks this in a child process. The
typed `Renderer` makes it impossible, since every call goes through a configured renderer.

## Known renderer issues (found while testing)

- Between `set_render_config` and `init`, `create_model` doesn't report `NotInitialized`. It gets past the
  context check and asserts inside VMA (null allocator). So "create_model before init returns NotInitialized"
  can't be tested.
- With `manual_frame_presentation_and_submission`, `render_frame` returns OK for a frame it skipped (the
  swapchain was out of date and has been recreated), and the caller can't tell. Submitting and presenting it
  anyway (C++ code does this too, and so does the typed frame guard) submits a frame that wasn't recorded.
  Also, in SceneMode without a bound scene, `render_frame` records an empty frame but returns `NoSceneBound`,
  so the guard doesn't submit it. Bind a scene before the first frame.
