// The NRE DSL: a scope that owns what it creates and a frame loop. Import with `import nre.kt.dsl.*` (and
// `import nre.kt.*` for the handle properties, camera, input and math).
//
//     fun main() = nre(800, 600, "demo") {
//         pipeline(vertex = shaders / "vertex.vert.spv", fragment = shaders / "fragment.frag.spv")
//         val car = instance(assets / "police.glb") { position = vec3(0, 0, -1) }
//         loop { dt -> car.rotate(dt, Vec3.UP) }
//     }
package nre.kt.dsl

import nre.ApiMode
import nre.Cubemap
import nre.Handle
import nre.Instance
import nre.Light
import nre.Material
import nre.Model
import nre.NRE
import nre.Pipeline
import nre.RenderErrorPolicy
import nre.RenderMode
import nre.Scene
import nre.SceneLighting
import nre.Shader
import nre.ShaderType
import nre.Texture
import nre.Vec3
import nre.Vertex
import nre.kt.setRenderConfig
import java.nio.file.Files
import java.nio.file.NoSuchFileException
import java.nio.file.Path
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

/** Marks the DSL's receivers, so a builder block can't reach the scope around it by accident. */
@DslMarker
annotation class NreDsl

private var running = false

/** Set by the JVM shutdown hook (SIGTERM, Ctrl+C): makes loop { } stop as if the window was closed. */
@Volatile
internal var exitRequested = false

/** How long the shutdown hook waits for the scope to clean up on the main thread. */
private const val EXIT_WAIT_SECONDS = 5L

/**
 * Configures and starts the renderer (the parameters are [setRenderConfig]'s), runs [block], and then, however
 * the block ends, destroys everything created through the [NreScope] (newest first) and calls [NRE.shutdown].
 *
 * Lifetimes are tied to this scope, never to the garbage collector. Handles made with raw `NRE.createXxx` calls
 * aren't owned: destroy them yourself, or hand them to [NreScope.track]. In SceneMode the scope creates (and owns)
 * a scene first, since nearly everything needs a bound one; FunctionDrawMode's init already binds one.
 *
 * Call it from the main thread, and once at a time (it's one renderer per process). If init fails, its exception
 * is thrown and shutdown isn't called.
 *
 * When the JVM is asked to exit (SIGTERM, Ctrl+C) while the scope runs, a shutdown hook stops [NreScope.loop] and
 * waits (up to 5 s) for the scope to clean up on the thread that runs it; without that the process exits while the main
 * thread is still rendering. Don't call `exitProcess` inside the block: return from it, or `close()` the loop.
 */
fun nre(
    width: Int,
    height: Int,
    title: String,
    mode: RenderMode = RenderMode.RENDER_3D,
    apiMode: ApiMode = ApiMode.SCENE,
    errorPolicy: RenderErrorPolicy = RenderErrorPolicy.RETURN,
    manualFramePresentationAndSubmission: Boolean = false,
    resizable: Boolean = true,
    showFpsInTitle: Boolean = true,
    block: NreScope.() -> Unit,
) {
    check(!running) { "nre { } is already running: there is one renderer per process" }
    running = true
    exitRequested = false
    val done = CountDownLatch(1)
    val hook = Thread({
        exitRequested = true
        done.await(EXIT_WAIT_SECONDS, TimeUnit.SECONDS)
    }, "nre-exit")
    Runtime.getRuntime().addShutdownHook(hook)
    try {
        setRenderConfig(
            width, height, title, mode, apiMode, errorPolicy, manualFramePresentationAndSubmission, resizable,
            showFpsInTitle,
        )
        NRE.init()
        val scope = NreScope(mode, apiMode, manualFramePresentationAndSubmission)
        runScope(scope.tracker, NRE::shutdown) {
            if (apiMode == ApiMode.SCENE) scope.scene()
            scope.block()
        }
    } finally {
        running = false
        done.countDown()
        try {
            Runtime.getRuntime().removeShutdownHook(hook)
        } catch (_: IllegalStateException) {
            // The JVM is already exiting: the hook has run or is running.
        }
    }
}

/**
 * The receiver of [nre]: creates resources it owns, and runs the frame [loop]. Everything here can also be called
 * inside `loop { }`; nested builder blocks (`instance { }`, `material { }`) only see their own builder.
 */
@NreDsl
class NreScope internal constructor(
    val mode: RenderMode,
    val apiMode: ApiMode,
    private val manualFramePresentationAndSubmission: Boolean,
) {
    internal val tracker = Tracker()
    private var looping = false

    // -----------------------------------------------------------------------------------------------------------
    // Ownership
    // -----------------------------------------------------------------------------------------------------------

    /** Makes the scope own [handle] (e.g. one from a raw NRE call): it's destroyed when the scope ends. */
    fun <H : Handle> track(handle: H): H = tracker.track(handle) { destroyHandle(it) }

    /**
     * Destroys an owned resource now, and stops owning it, so it isn't destroyed twice. Throws
     * IllegalArgumentException for a handle the scope doesn't own (raw, or already destroyed).
     */
    fun destroy(handle: Handle) = tracker.destroy(handle)

    private inline fun <H : Handle> own(create: () -> H): H {
        tracker.checkOpen()
        return track(create())
    }

    // -----------------------------------------------------------------------------------------------------------
    // Shaders and pipelines
    // -----------------------------------------------------------------------------------------------------------

    /** A SPIR-V shader. Checks the file exists first: NRE doesn't, and a pipeline made from it then crashes. */
    fun shader(path: Path, type: ShaderType): Shader {
        if (!Files.isRegularFile(path)) throw NoSuchFileException(path.toString(), null, "shader file not found")
        return own { NRE.createShader(path.toString(), type) }
    }

    /** A pipeline from two shaders, bound unless [bind] is false. */
    fun pipeline(vertex: Shader, fragment: Shader, bind: Boolean = true): Pipeline =
        own { NRE.createPipeline(vertex, fragment) }.also { if (bind) NRE.bindPipeline(it) }

    /** Loads both shaders (owned too) and makes a pipeline from them, bound unless [bind] is false. */
    fun pipeline(vertex: Path, fragment: Path, bind: Boolean = true): Pipeline =
        pipeline(shader(vertex, ShaderType.VERTEX), shader(fragment, ShaderType.FRAGMENT), bind)

    // -----------------------------------------------------------------------------------------------------------
    // Models and instances
    // -----------------------------------------------------------------------------------------------------------

    /** Loads a model file (glTF, OBJ, ...). Draw it through instances, or with `draw(model)` in FunctionDrawMode. */
    fun model(path: Path): Model = own { NRE.createModel(path.toString()) }

    /**
     * A model from your own triangles: every three [indices] are one triangle (counter-clockwise from the front).
     * Zero normals are computed from the triangles; [Material.NONE] is the default material. Copied, so the
     * arrays can be reused.
     */
    fun meshModel(vertices: List<Vertex>, indices: IntArray, material: Material = Material.NONE): Model =
        own { NRE.createMeshModel(vertices, indices, material) }

    /** [meshModel] from the vertices and triangles added in [block]: `mesh { val a = vertex(...); triangle(a, b, c) }`. */
    fun mesh(material: Material = Material.NONE, block: MeshBuilder.() -> Unit): Model {
        val builder = MeshBuilder().apply(block)
        return meshModel(builder.vertices, builder.indexArray(), material)
    }

    /**
     * A new instance of [model], set up by [block], and added to the bound scene if [addToScene] (by default in
     * SceneMode only: FunctionDrawMode draws instances with `draw(instance)`, not from the scene).
     */
    fun instance(
        model: Model,
        addToScene: Boolean = apiMode == ApiMode.SCENE,
        block: InstanceBuilder.() -> Unit = {},
    ): Instance {
        val builder = InstanceBuilder().apply(block)
        val instance = own { NRE.createInstance(model) }
        builder.applyTo(instance)
        if (addToScene) NRE.addInstanceToScene(instance)
        return instance
    }

    /** Loads the model at [path] (owned too; each call loads it again) and makes one instance of it. */
    fun instance(
        path: Path,
        addToScene: Boolean = apiMode == ApiMode.SCENE,
        block: InstanceBuilder.() -> Unit = {},
    ): Instance = instance(model(path), addToScene, block)

    /** `model.instance { position = ... }`: see [instance]. */
    @JvmName("instanceOf")
    fun Model.instance(
        addToScene: Boolean = apiMode == ApiMode.SCENE,
        block: InstanceBuilder.() -> Unit = {},
    ): Instance = instance(this, addToScene, block)

    // -----------------------------------------------------------------------------------------------------------
    // Textures and materials
    // -----------------------------------------------------------------------------------------------------------

    /**
     * Loads an image. [srgb]: true (the default, like renderer.h) for colour images; false for data maps, which hold
     * plain numbers (normal and metallic-roughness maps). In `material { }`, `normal(path)` and `metallic(path)`
     * load those with srgb = false.
     */
    fun texture(path: Path, srgb: Boolean = true): Texture = own { NRE.createTexture(path.toString(), srgb) }

    /** A material from the textures set in [block] (unset ones are empty: white base, not metal, flat). */
    fun material(block: MaterialBuilder.() -> Unit = {}): Material {
        val builder = MaterialBuilder(this).apply(block)
        return own { NRE.createMaterial(builder.base, builder.metallic, builder.normal) }
    }

    // -----------------------------------------------------------------------------------------------------------
    // Cubemaps and the skybox
    // -----------------------------------------------------------------------------------------------------------

    /** A cubemap from six square images of the same size. */
    fun cubemap(right: Path, left: Path, top: Path, bottom: Path, front: Path, back: Path): Cubemap = own {
        NRE.createCubemap(
            right.toString(), left.toString(), top.toString(), bottom.toString(), front.toString(), back.toString(),
        )
    }

    /** A cubemap from `right.png`, `left.png`, `top.png`, `bottom.png`, `front.png` and `back.png` in [dir]. */
    fun cubemap(dir: Path, extension: String = "png"): Cubemap {
        val f = FACES.map { dir.resolve("$it.$extension") }
        return cubemap(f[0], f[1], f[2], f[3], f[4], f[5])
    }

    /** [cubemap] from [dir], set as the bound scene's skybox (3D only). */
    fun skybox(dir: Path, extension: String = "png"): Cubemap = cubemap(dir, extension).also(NRE::setSkybox)

    /** [cubemap] from the six faces, set as the bound scene's skybox (3D only). */
    fun skybox(right: Path, left: Path, top: Path, bottom: Path, front: Path, back: Path): Cubemap =
        cubemap(right, left, top, bottom, front, back).also(NRE::setSkybox)

    // -----------------------------------------------------------------------------------------------------------
    // Scenes and lights
    // -----------------------------------------------------------------------------------------------------------

    /** A new scene. The first scene created is bound automatically; otherwise it's bound only if [bind]. */
    fun scene(bind: Boolean = false): Scene = own { NRE.createScene() }.also { if (bind) NRE.bindScene(it) }

    /** A point light, added to the bound scene unless [addToScene] is false. Falls off with 1/distance². */
    fun pointLight(position: Vec3, color: Vec3 = Vec3.ONE, intensity: Float = 10f, addToScene: Boolean = true): Light =
        own { NRE.createPointLight(position, color, intensity) }.also { if (addToScene) NRE.addLightToScene(it) }

    /** A directional light ([direction] is the way it travels), added to the bound scene unless [addToScene] is false. */
    fun directionalLight(
        direction: Vec3,
        color: Vec3 = Vec3.ONE,
        intensity: Float = 3f,
        addToScene: Boolean = true,
    ): Light = own { NRE.createDirectionalLight(direction, color, intensity) }
        .also { if (addToScene) NRE.addLightToScene(it) }

    /** Sets the bound scene's sun (not a resource: nothing to destroy). `sunDirection` moves it later. */
    fun sun(direction: Vec3, color: Vec3 = Vec3.ONE, intensity: Float = 3f) = NRE.setSun(direction, color, intensity)

    /**
     * The bound scene's lighting mode: `lighting = SceneLighting.LANDSCAPE`. Setting it resets the shadow distance,
     * haze and sky to the mode's defaults. `Lighting` (nre.kt) changes the rest later, e.g. in the loop.
     */
    var lighting: SceneLighting
        get() = NRE.getSceneLighting()
        set(value) = NRE.setSceneLighting(value)

    /**
     * Sets the bound scene's lighting [mode] (which resets shadows, haze and sky to its defaults), then what [block]
     * changes: `lighting(SceneLighting.SMALL) { shadowDistance = 40f; skyColor = vec3(0.2, 0.3, 0.5) }`.
     */
    fun lighting(mode: SceneLighting, block: LightingBuilder.() -> Unit = {}) {
        val builder = LightingBuilder().apply(block)
        NRE.setSceneLighting(mode)
        builder.apply()
    }

    /** `lighting(SceneLighting.LANDSCAPE) { }`: `landscape { exposure = 1.2f; haze(5000f, height = 200f) }`. */
    fun landscape(block: LightingBuilder.() -> Unit = {}) = lighting(SceneLighting.LANDSCAPE, block)

    // -----------------------------------------------------------------------------------------------------------
    // The frame loop
    // -----------------------------------------------------------------------------------------------------------

    /**
     * Runs until the window is closed or [LoopScope.close] is called. Each iteration: pollEvents, then [block]
     * with the seconds the last frame took, then the frame is rendered unless the block called
     * [LoopScope.render] itself. So camera, light and transform changes in the block show in this frame.
     *
     * FunctionDrawMode: draw in `render { draw(...) }` inside the block. Not supported with
     * manualFramePresentationAndSubmission: write that loop yourself with NRE.renderFrame/submitFrame/presentFrame.
     */
    fun loop(block: LoopScope.(dt: Float) -> Unit) {
        check(!manualFramePresentationAndSubmission) {
            "loop { } doesn't support manualFramePresentationAndSubmission = true: the C API can't tell it whether " +
                "renderFrame skipped a frame (e.g. while resizing), so it can't call submitFrame/presentFrame " +
                "safely. Write the loop yourself with NRE.renderFrame/endFrame/submitFrame/presentFrame."
        }
        check(!looping) { "loop { } is already running" }
        tracker.checkOpen()
        looping = true
        try {
            LoopScope(apiMode, NativeFrameApi).run(block)
        } finally {
            looping = false
        }
    }

    private companion object {
        val FACES = listOf("right", "left", "top", "bottom", "front", "back")
    }
}
