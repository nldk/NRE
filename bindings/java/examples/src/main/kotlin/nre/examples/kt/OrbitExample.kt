// Port of example/main.cpp in Kotlin: a police car on the plains skybox, slowly rotating, with an orbit camera
// (drag with the left mouse button to orbit, scroll to zoom). Pass "scene" as the first argument for SceneMode;
// the default is FunctionDrawMode, like main.cpp.
//
// Run: ./gradlew :examples:runKotlin (needs NRE_LIB_DIR, see the README). Shaders come from $NRE_SHADER_DIR,
// by default <repo>/cmake-build-debug/example/shaders.
package nre.examples.kt

import nre.ApiMode
import nre.MouseButton
import nre.NRE.*
import nre.RenderErrorPolicy
import nre.ShaderType
import nre.Vec3
import nre.kt.*
import nre.kt.setRenderConfig // explicit, so it wins over the Java overloads from nre.NRE.*
import java.nio.file.Files
import java.nio.file.Path
import kotlin.io.path.Path
import kotlin.math.cos
import kotlin.math.pow
import kotlin.math.sin

/** The camera lives in the app, not in the bindings: it only uses NRE's input and camera functions. */
class OrbitCamera(var target: Vec3 = Vec3.ZERO) {
    var distance = 3.0f
    var yaw = 0.0f
    var pitch = 0.0f

    fun update() {
        if (isMouseButtonDown(MouseButton.LEFT)) {
            val (dx, dy) = getMouseDelta()
            yaw -= dx * ROTATE_SPEED
            pitch = (pitch + dy * ROTATE_SPEED).coerceIn(-MAX_PITCH, MAX_PITCH)
        }

        distance = (distance * ZOOM_STEP.pow(getScrollDelta())).coerceAtLeast(0.1f)

        val offset = Vec3(cos(pitch) * sin(yaw), sin(pitch), cos(pitch) * cos(yaw))
        setCameraPosition(target + offset * distance)
        cameraLookAt(target)
    }

    companion object {
        const val ROTATE_SPEED = 0.01f
        const val ZOOM_STEP = 0.9f
        const val MAX_PITCH = 1.55f
    }
}

fun main(args: Array<String>) {
    val apiMode = if (args.firstOrNull() == "scene") ApiMode.SCENE else ApiMode.FUNCTION_DRAW
    val repo = findRepo()
    val assets = repo.resolve("example/assets")
    val shaders = System.getenv("NRE_SHADER_DIR")?.takeIf { it.isNotEmpty() }?.let { Path(it) }
        ?: repo.resolve("cmake-build-debug/example/shaders")

    setRenderConfig(500, 500, "test", apiMode = apiMode, errorPolicy = RenderErrorPolicy.IGNORE)
    init()
    val model = createModel(assets.resolve("police.glb").toString())
    val car = createInstance(model)
    val vertex = createShader(shaders.resolve("vertex.vert.spv").toString(), ShaderType.VERTEX)
    val frag = createShader(shaders.resolve("fragment.frag.spv").toString(), ShaderType.FRAGMENT)
    val pipeline = createPipeline(vertex, frag)
    bindPipeline(pipeline)
    createScene()
    addInstanceToScene(car)
    translateInstance(car, Vec3(0.0f, 0.0f, -1.0f))
    val sky = assets.resolve("skyboxes/plains")
    val faces = listOf("right", "left", "top", "bottom", "front", "back").map { sky.resolve("$it.png").toString() }
    val skybox = createCubemap(faces[0], faces[1], faces[2], faces[3], faces[4], faces[5])
    setSkybox(skybox)

    val camera = OrbitCamera(getInstanceTransform(car).position())
    while (!windowShouldClose()) {
        pollEvents()
        camera.update()
        if (apiMode == ApiMode.SCENE) {
            renderFrame()
        } else {
            renderFrame()
            drawInstance(car)
            endFrame()
        }

        rotateInstance(car, 0.0001f, Vec3(0.0f, 1.0f, 0.0f))
    }
    shutdown()
}

/** The repository root, found from where this code was loaded (not the working directory). */
private fun findRepo(): Path {
    val location = OrbitCamera::class.java.protectionDomain.codeSource.location
    return generateSequence(Path.of(location.toURI())) { it.parent }
        .firstOrNull { Files.isRegularFile(it.resolve("include/nre.h")) }
        ?: error("can't find the NRE repository above $location")
}
