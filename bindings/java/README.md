# NRE for Java and Kotlin

Java bindings for NRE's C API (`include/nre.h`), written with the Foreign Function & Memory API
(`java.lang.foreign`). No JNI, no C glue, no JNA. There's also a small Kotlin add-on.

- `nre`: the Java library, package `nre`. `NRE` has one static method per nre.h function, named like
  renderer.h (`nreCreateModel` is `NRE.createModel`). Errors throw `NreException` (`code()`, `nreMessage()`).
  Handles (`Model`, `Instance`, ...) are records you destroy yourself; `Xxx.NONE` is the empty handle.
  Math: `Vec2`, `Vec3`, `Quat`, `Transform` (records, with `IDENTITY`/`ZERO`/`ONE`) and `Mat4` (column-major).
  Out-parameter structs are records too: `Vertex` (for `createMeshModel`) and `RayHit` (from `pickInstance`, which
  returns `Optional<RayHit>`).
- `nre-kotlin`: `import nre.kt.*` for `+ - * / unary-` on `Vec2`/`Vec3`, destructuring
  (`val (x, y) = getMouseDelta()`), a `setRenderConfig` with named default parameters, properties on the handles
  (`car.position`), `Camera`, `Mouse`, `Key.W.isDown` and math helpers; `import nre.kt.dsl.*` for the DSL
  (see [The Kotlin DSL](#the-kotlin-dsl)).
- `examples`: `example/main.cpp` ported to Java (`OrbitExample.java`), Kotlin (`OrbitExample.kt`) and the Kotlin
  DSL (`OrbitDsl.kt`).

Call NRE from the thread that called `init()` (GLFW needs the main thread; on macOS add `-XstartOnFirstThread`),
and not from virtual threads: the last error message is per OS thread.

## Requirements

- JDK 25 to build (the code needs Java 22+; the build uses a Java 25 toolchain). Gradle finds installed JDKs
  itself (e.g. `/usr/lib/jvm/java-25-openjdk`); if it doesn't, add
  `org.gradle.java.installations.paths=/path/to/jdk-25` to `~/.gradle/gradle.properties`.
- Nothing else: `./gradlew` downloads Gradle 9.9.0, Kotlin 2.4.21 and JUnit.
- libnre_c 0.3.x, built with the main CMake project (`cmake-build-debug/bin/libnre_c.so.0.3`).

## Pointing it at libnre_c

If `NRE_LIB_DIR` is set, the library is loaded from that directory; otherwise by name
(`libnre_c.so.0.3` / `libnre_c.0.3.dylib` / `nre_c.dll`) through the system's library search, e.g.
`LD_LIBRARY_PATH`. It must report version 0.3.x, or loading fails with an `UnsatisfiedLinkError` (a 0.2 library is
rejected, even under the 0.3 name).

## What's new in 0.3

- **Breaking:** `createTexture(path, srgb)`. `createTexture(path)` is still there and loads sRGB, as before and like
  renderer.h's default; data maps (normal, metallic-roughness) need `createTexture(path, false)`.
- `createMeshModel(vertices, indices[, material])`: a model from your own triangles. `vertices` is a `Vertex[]` or a
  `List<Vertex>`, `indices` an `int[]` (read as unsigned); each is copied into one native buffer per call. A
  `Vertex` with a zero normal gets one computed from its triangles.
- `setSceneLighting(SceneLighting.SMALL / LANDSCAPE)` / `getSceneLighting()`, `setShadowDistance(meters)`,
  `setHaze(visibility[, height = 300])`, `setExposure`, `setSkyColor`, `setCastsShadows(instance, bool)`.
- `getProjectionMatrix()`, `pickInstance(windowPos)`: `Optional<RayHit>` (instance, distance, point).
- `ErrorCode.INVALID_ARGUMENT` (e.g. a negative shadow distance, a mesh index out of range in Debug builds).

The JVM needs `--enable-native-access=ALL-UNNAMED` (the Gradle tasks below pass it).

## Running

```sh
export NRE_LIB_DIR=/path/to/VulkanRenderer/cmake-build-debug/bin

./gradlew :nre:test                         # headless: no window needed
./gradlew :examples:runJava                 # FunctionDrawMode, like main.cpp
./gradlew :examples:runJava --args=scene    # SceneMode
./gradlew :examples:runKotlin               # same, in Kotlin
./gradlew :examples:runKotlinDsl            # same, with the Kotlin DSL (--args=scene works here too)
./gradlew :nre-kotlin:test                  # headless; NRE_WINDOW_TESTS=1 adds tests that open a window
```

The examples load assets from `<repo>/example/assets` and shaders from `$NRE_SHADER_DIR`, by default
`<repo>/cmake-build-debug/example/shaders` (build the main project first). Drag with the left mouse button
to orbit, scroll to zoom.

Without Gradle: `java --enable-native-access=ALL-UNNAMED -cp nre/build/libs/nre-0.3.0.jar:... YourMain`.

## The Kotlin DSL

`nre.kt.dsl` makes NRE look like a Kotlin library: a scope that owns what it creates, builders, and a frame loop.

```kotlin
import nre.*
import nre.kt.*
import nre.kt.dsl.*
import kotlin.io.path.Path
import kotlin.io.path.div   // Path / "child"

val assets = Path("example/assets")
val shaders = Path("cmake-build-debug/example/shaders")

fun main() = nre(500, 500, "test", apiMode = ApiMode.FUNCTION_DRAW) {   // setRenderConfig's parameters
    pipeline(vertex = shaders / "vertex.vert.spv", fragment = shaders / "fragment.frag.spv")   // and binds it
    skybox(assets / "skyboxes/plains")             // right/left/top/bottom/front/back.png, set as the skybox
    sun(direction = vec3(-1, -1, 0))
    val car = instance(assets / "police.glb") { position = vec3(0, 0, -1); rotate(30.deg, Vec3.UP) }
    pointLight(position = vec3(0, 2, 0), intensity = 20f)
    loop { dt ->
        Camera.lookAt(car.position)               // before the frame: this frame uses it
        if (Key.ESCAPE.isPressed) close()
        render { draw(car) }                      // FunctionDrawMode: renderFrame, the draws, endFrame
        car.rotate(0.4f * dt, Vec3.UP)
    }
}
```

**Lifetime.** `nre(...) { }` calls `setRenderConfig` and `init`, runs the block, and then, however the block ends
(normally or by an exception), destroys everything created through the scope, newest first, and calls `shutdown`.
Nothing depends on the garbage collector. Handles from raw `NRE.createXxx` calls aren't owned: destroy them
yourself or pass them to `track(handle)`. `destroy(handle)` destroys an owned resource early and forgets it, so
it can't be destroyed twice (a second `destroy` throws). A block exception wins; cleanup failures are attached
to it as suppressed exceptions. If `init` fails, `shutdown` isn't called. SIGTERM / Ctrl+C stop the loop and let
the scope clean up on the main thread (a shutdown hook waits for it up to 5 s); don't call `exitProcess` inside
the block. In SceneMode the scope creates and owns a scene first (FunctionDrawMode's `init` already binds one), since
the camera, sun, skybox and lights all need a bound scene.

**What the scope creates** (all owned): `shader`, `pipeline(vertex, fragment, bind = true)` (from paths or
shaders; checks the files exist, since NRE doesn't), `model`, `meshModel(vertices, indices, material)`,
`mesh(material) { val a = vertex(position, normal, uv); triangle(a, b, c); quad(a, b, c, d) }` (`vertex` returns
the index), `instance(model or path) { }` / `model.instance { }` (added to the bound scene by default in SceneMode
only), `texture(path, srgb = true)`, `material { base(path); metallic(path); normal(path) }`, `cubemap` / `skybox`
(a directory, or the six faces by name), `scene(bind = false)`, `pointLight` / `directionalLight` (added to the bound
scene unless `addToScene = false`), and `sun(...)`, which isn't a resource.

**sRGB and data maps.** Colour images are sRGB, data maps aren't (renderer.h: normal and metallic-roughness maps
need `srgb = false`). In `material { }`, `metallic(path)` and `normal(path)` load with `srgb = false` and `base(path)`
with `srgb = true`. `texture(path)` defaults to `srgb = true` like renderer.h, so `normal = texture(path)` would be
wrong: write `normal(path)`, or `normal = texture(path, srgb = false)`.

**Lighting** (on the bound scene; not resources): `lighting = SceneLighting.LANDSCAPE` (get and set), or
`lighting(SceneLighting.SMALL) { ... }` / `landscape { exposure = 1.2f; shadowDistance = 200f; haze(5000f, height = 200f);
skyColor = vec3(...) }`, which sets the mode (resetting shadows, haze and sky to its defaults) and then what the block
changes. Later changes, e.g. in the loop, go through `Lighting` (below). Lights take named parameters, not a builder: they're fully set at creation. The `instance { }` block
sets `position`, `rotation`, `scale`, `transform`, `visible`, `translate(...)` and `rotate(angle, axis)`; it runs
before the instance is created. Builder blocks can't reach the scope around them (`@DslMarker`).

**The loop.** `loop { dt -> }` runs until the window closes or `close()` is called (the C API has no "set should
close", so `close()` only ends the loop). Each iteration: `pollEvents`, the block with `getDeltaTime()`, then
the frame. renderFrame reads the bound scene's camera and lights when it starts the frame, and in FunctionDrawMode
the draws must come between renderFrame and endFrame, each one copying the instance's transform as it is then.
So the block runs before the frame: update the camera, lights and transforms there, then call `render { draw(...) }`
(FunctionDrawMode only: renderFrame, the block, endFrame, also when the block throws). `draw(instance)`,
`draw(model, transform)` and `draw(model, position = ..., rotation = ..., scale = ...)` only exist inside
`render { }`, so drawing anywhere else doesn't compile; `pick()` (under the mouse) / `pick(windowPos)` returns the
`RayHit?` under a pixel; `render { }` in SceneMode throws. If the block doesn't call
`render`, the loop renders after it (in FunctionDrawMode, an empty frame); `render()` without a block renders at a
point of your choosing. One frame per iteration. `loop { }` rejects `manualFramePresentationAndSubmission = true`:
the C API can't tell it whether a frame was skipped, so it can't call submitFrame/presentFrame safely; write that loop
yourself.

**Outside the DSL too** (`nre.kt`, plain NRE calls on any handle): `instance.transform` / `position` / `rotation` /
`scale` (get and set), `matrix`, `translate`, `rotate(angle, axis)`, `scaleBy`, `setVisible`, `setCastsShadows`, `addToScene`;
`model.materials`, `setMaterial`, `replaceMaterial`; `material.setBaseTexture`; light `setPosition` /
`setDirection` / `setColor(color, intensity)` (functions: nre.h has no getters); `cubemap.useAsSkybox()`;
`scene.bind()`; `sunDirection`, `fps`; `Lighting.mode` (get and set) / `setShadowDistance` / `setHaze(visibility,
height = 300f)` / `setExposure` / `setSkyColor`; `pick(Mouse.position)` (a `RayHit?`, destructurable:
`val (instance, distance, point) = hit`); `Camera.position` / `forward` / `viewMatrix` / `projectionMatrix` / `lookAt` / `move` /
`moveRelative` (x right, y up, z forward) / `rotate` / `setRotation`; `Mouse.delta` / `position` / `scroll` /
`setCursorMode`; `Key.W.isDown` / `isPressed` / `isReleased` and the same on `MouseButton`. Math: `vec3(1, 0, -2.5)`
(any `Number`), `vec3(2)`, `90.deg` (radians), `quat(angle, axis)`, `q1 * q2`, `q * v`, `length`, `normalized()`,
`dot`, `cross`, `transform(position = ...)`, `vertex(position, normal = ZERO, uv = ZERO)`, `Vec3.UP` / `RIGHT` /
`FORWARD` (-Z, the camera's forward at yaw 0).

Running an example without Gradle (faster; build the jars first with `./gradlew jar`):

```sh
NRE_LIB_DIR=$PWD/../../cmake-build-debug/bin java --enable-native-access=ALL-UNNAMED \
  -cp nre/build/libs/nre-0.3.0.jar:nre-kotlin/build/libs/nre-kotlin-0.3.0.jar:examples/build/libs/examples-0.3.0.jar:$KOTLIN_STDLIB \
  nre.examples.kt.OrbitDslKt scene
```

where `$KOTLIN_STDLIB` is `kotlin-stdlib-2.4.21.jar` from `~/.gradle/caches/modules-2/files-2.1/org.jetbrains.kotlin/`.
