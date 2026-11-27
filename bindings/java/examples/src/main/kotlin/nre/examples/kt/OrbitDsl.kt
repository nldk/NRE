// example/main.cpp again, written with the Kotlin DSL (nre.kt.dsl): a police car on the plains skybox, slowly
// rotating, with an orbit camera (drag with the left mouse button to orbit, scroll to zoom, Escape to quit).
// Pass "scene" as the first argument for SceneMode; the default is FunctionDrawMode, like main.cpp.
//
// Run: ./gradlew :examples:runKotlinDsl (needs NRE_LIB_DIR, see the README). Shaders come from $NRE_SHADER_DIR,
// by default <repo>/cmake-build-debug/example/shaders.
package nre.examples.kt

import nre.ApiMode
import nre.Key
import nre.MouseButton
import nre.Vec3
import nre.kt.*
import nre.kt.dsl.*
import java.nio.file.Files
import java.nio.file.Path
import kotlin.io.path.Path
import kotlin.io.path.div
import kotlin.math.cos
import kotlin.math.pow
import kotlin.math.sin

/** The orbit camera is app code, not part of the library: it only uses the camera and input. */
class Orbit(var target: Vec3, var distance: Float = 3f) {
    private var yaw = 0f
    private var pitch = 0f

    fun update() {
        if (MouseButton.LEFT.isDown) {
            val (dx, dy) = Mouse.delta
            yaw -= dx * ROTATE_SPEED
            pitch = (pitch + dy * ROTATE_SPEED).coerceIn(-MAX_PITCH, MAX_PITCH)
        }
        distance = (distance * ZOOM_STEP.pow(Mouse.scroll)).coerceAtLeast(0.1f)

        val offset = vec3(cos(pitch) * sin(yaw), sin(pitch), cos(pitch) * cos(yaw))
        Camera.position = target + offset * distance
        Camera.lookAt(target)
    }

    private companion object {
        const val ROTATE_SPEED = 0.01f
        const val ZOOM_STEP = 0.9f
        const val MAX_PITCH = 1.55f
    }
}

fun main(args: Array<String>) {
    val apiMode = if (args.firstOrNull() == "scene") ApiMode.SCENE else ApiMode.FUNCTION_DRAW
    val repo = repoRoot()
    val assets = repo / "example/assets"
    val shaders = System.getenv("NRE_SHADER_DIR")?.takeIf { it.isNotEmpty() }?.let { Path(it) }
        ?: (repo / "cmake-build-debug/example/shaders")

    nre(500, 500, "test", apiMode = apiMode) {
        pipeline(vertex = shaders / "vertex.vert.spv", fragment = shaders / "fragment.frag.spv")
        skybox(assets / "skyboxes/plains")
        val car = instance(assets / "police.glb") { position = vec3(0, 0, -1) }   // in the scene in SceneMode
        val orbit = Orbit(target = car.position)

        loop { dt ->
            orbit.update()                     // before the frame starts, so this frame uses it
            if (Key.ESCAPE.isPressed) close()
            if (apiMode == ApiMode.FUNCTION_DRAW) {
                render { draw(car) }
            }                                  // SceneMode: the loop renders the scene after the block
            car.rotate(0.4f * dt, Vec3.UP)     // radians per second
        }
    }
}

/** The repository root, found from where this code was loaded (not the working directory). */
private fun repoRoot(): Path {
    val location = Orbit::class.java.protectionDomain.codeSource.location
    return generateSequence(Path.of(location.toURI())) { it.parent }
        .firstOrNull { Files.isRegularFile(it.resolve("include/nre.h")) }
        ?: error("can't find the NRE repository above $location")
}
