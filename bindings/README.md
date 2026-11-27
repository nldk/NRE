# NRE language bindings

Bindings for the C API (`include/nre.h`, `libnre_c`) in other languages. Each one wraps every function in
`nre.h`, has a headless test (no window) and a port of `example/main.cpp` (police car, plains skybox, orbit
camera: drag with the left mouse button, scroll to zoom). See each folder's README for details.

| Language | Folder | How it calls C | Test | Example |
|---|---|---|---|---|
| Python ≥ 3.10 | `python/` | ctypes, pure Python | `python3 -m unittest` | `python3 examples/orbit.py` |
| Rust | `rust/` | `nre-sys` (raw) + `nre` (safe + typed `Renderer`) | `cargo test` | `cargo run --example orbit` / `orbit_typed` |
| Java ≥ 22 / Kotlin | `java/` | FFM (`java.lang.foreign`), no JNI; Kotlin extensions + DSL | `./gradlew build` | `./gradlew :examples:runJava` / `:runKotlin` / `:runKotlinDsl` |
| C# / .NET 10 | `csharp/` | `[LibraryImport]` P/Invoke | `dotnet test` | `dotnet run --project Nre.Examples.Orbit` |
| Go | `go/` | cgo | `make test` | `make run` |
| Swift 6 | `swift/` | SwiftPM system library over `nre.h` | `swift test` | `swift run OrbitExample` |
| Zig 0.17 | `zig/` | `addTranslateC` on `nre.h` | `zig build test` | `zig build run` |
| Odin | `odin/` | `foreign import` | `./run.sh test` | `./run.sh orbit` |
| Lua (LuaJIT only) | `lua/` | LuaJIT FFI, pure Lua | `luajit tests/headless.lua` | `luajit examples/orbit.lua` |

Run every command from the binding's own folder, after building libnre_c and pointing `NRE_LIB_DIR` at it:

```sh
cmake --build cmake-build-debug --target nre_c VulkanRenderer   # libnre_c + the example's shaders
export NRE_LIB_DIR=$PWD/cmake-build-debug/bin                   # absolute path
```

The examples read shaders from `NRE_SHADER_DIR`, default `cmake-build-debug/example/shaders`.

Rust and Kotlin also have a higher-level layer on top of the plain bindings:

- **Rust, `nre::Renderer<SceneMode | DrawMode>`**: setup, frames and shutdown as types. Calling the API before
  `init`, drawing in SceneMode, two frames at once or a frame outliving the renderer don't compile; dropping a
  frame ends it, dropping the renderer shuts down. Optional `glam` / `mint` conversions. See `rust/README.md`.
- **Kotlin, `nre(w, h, title) { ... }`**: a DSL with scoped cleanup (everything created in the block is destroyed,
  then `shutdown()`), builders (`instance(...) { position = ... }`, `material { }`, lights, `skybox`, `pipeline`),
  properties on handles (`car.position += ...`, `Key.W.isDown`) and a frame loop (`loop { dt -> render { draw(car) } }`).
  See `java/README.md`.

## Shared conventions

- **Names** are the C++ names in the language's style: `create_model`, `createModel`, `CreateModel`.
- **Errors**: a failed call becomes the language's error (exception, `Result`, `error`, error union,
  `(value, Error_Code)`), with the error code and the message from `nreGetLastErrorMessage()`. Unknown codes
  map to `Unknown`. Out-parameters are return values.
- **Handles** are small copyable values with explicit `destroy_*`, like the C++ API. The empty handle (all
  zeros) means "no texture" / "no skybox"; where the language has it, `None`/`nil`/`null`/`Option` is accepted.
- **Defaults** are plain parameters with defaults (or a short form plus a full form), no config structs:
  `set_render_config(width, height, title)` uses 3D, SceneMode, error policy Return, resizable, FPS in title.
- **Library loading**: from `NRE_LIB_DIR` if set, otherwise by name (`libnre_c.so.0.3`, `libnre_c.0.3.dylib`,
  `nre_c.dll`). Every binding checks that the library is version 0.3 (the C API they were written for) and fails
  clearly otherwise. A minor version bump while at 0.x is a breaking change, so a binding is updated with it.
- **Threads**: call NRE from the thread that called `init` (GLFW needs the main thread). The last-error
  message is per OS thread; Go locks the thread per call, Swift puts the API on `@MainActor`.

When `nre.h` changes, every binding has to follow. Go, Swift and Zig read `nre.h` itself, so a changed
signature is a compile error there. The Python, Java, C# and Lua tests parse `nre.h` and fail when a function
or enum value is missing. Rust and Odin copy the declarations by hand (their layout checks still catch struct
changes).

Only Linux has been tested. The Windows and macOS library names and link flags are written but untested.
