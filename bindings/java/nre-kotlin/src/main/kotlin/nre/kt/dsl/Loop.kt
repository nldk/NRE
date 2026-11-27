package nre.kt.dsl

import nre.ApiMode
import nre.Instance
import nre.Model
import nre.NRE
import nre.Quat
import nre.RayHit
import nre.Transform
import nre.Vec2
import nre.Vec3
import nre.kt.Mouse

/**
 * The receiver of [NreScope.loop]'s block, for one iteration at a time. Not marked with [NreDsl], so the
 * [NreScope] functions (creating and destroying resources) stay callable inside the loop.
 *
 * Where things go, following renderer.h: renderFrame reads the bound scene's camera and lights when it starts the
 * frame, and in FunctionDrawMode the draw calls must come between renderFrame and endFrame (each one copies the
 * instance's transform as it is then). So the block runs before the frame; set the camera, lights and transforms
 * there, then call `render { draw(...) }` for the draws. Anything after `render` lands in the next frame.
 */
class LoopScope internal constructor(val apiMode: ApiMode, private val api: FrameApi) {
    private var closed = false
    private var rendered = false
    private var active = false

    /** Ends the loop after this iteration (the C API has no "set should close"; the window stays open). */
    fun close() {
        closed = true
    }

    /**
     * The nearest visible instance of the bound scene under [windowPos] (by default the mouse), or null, with the
     * camera and transforms as they are now. Render_3D only; in FunctionDrawMode the scene usually holds no
     * instances, so nothing is hit.
     */
    fun pick(windowPos: Vec2 = Mouse.position): RayHit? = nre.kt.pick(windowPos)

    /**
     * Renders this iteration's frame now (once per iteration; the loop does it after the block otherwise).
     * SceneMode: draws the bound scene. FunctionDrawMode: an empty frame (sky only); use `render { }` to draw.
     */
    fun render() = frame(null)

    /**
     * FunctionDrawMode only: starts the frame, runs [draw] (the only place `draw(...)` exists), and ends the
     * frame, also when [draw] throws.
     */
    fun render(draw: DrawScope.() -> Unit) {
        check(apiMode == ApiMode.FUNCTION_DRAW) {
            "render { draw(...) } needs ApiMode.FUNCTION_DRAW (in SceneMode renderFrame draws the bound scene: " +
                "add instances to it, and call render() without a block, or let the loop render)"
        }
        frame(draw)
    }

    private fun frame(draw: (DrawScope.() -> Unit)?) {
        check(active) { "render is only valid inside loop { }" }
        check(!rendered) { "render was already called in this loop iteration: one frame per iteration" }
        rendered = true
        if (apiMode == ApiMode.SCENE) {
            api.renderFrame()
            return
        }
        // renderFrame leaves the frame open even when it fails (or skips the frame), so endFrame always follows.
        try {
            api.renderFrame()
            if (draw != null) {
                val scope = DrawScope(api)
                try {
                    scope.draw()
                } finally {
                    scope.open = false
                }
            }
        } catch (t: Throwable) {
            try {
                api.endFrame()
            } catch (e: Throwable) {
                t.addSuppressed(e)
            }
            throw t
        }
        api.endFrame()
    }

    internal fun run(block: LoopScope.(dt: Float) -> Unit) {
        active = true
        try {
            while (!closed && !api.windowShouldClose()) {
                api.pollEvents()
                rendered = false
                block(api.deltaTime())
                if (!rendered && !closed) frame(null)
            }
        } finally {
            active = false
        }
    }
}

/** The receiver of `render { }` in FunctionDrawMode: the frame is open, so the draw calls are valid here. */
@NreDsl
class DrawScope internal constructor(private val api: FrameApi) {
    internal var open = true

    /** Draws the instance with its current transform (a hidden one isn't drawn). */
    fun draw(instance: Instance) {
        checkOpen()
        api.drawInstance(instance)
    }

    /** Draws the model once with [transform], no instance needed. */
    fun draw(model: Model, transform: Transform) {
        checkOpen()
        api.drawModel(model, transform)
    }

    fun draw(model: Model, position: Vec3 = Vec3.ZERO, rotation: Quat = Quat.IDENTITY, scale: Vec3 = Vec3.ONE) =
        draw(model, Transform(position, rotation, scale))

    private fun checkOpen() = check(open) { "draw(...) is only valid inside render { }, while the frame is open" }
}

/** The frame calls the loop makes, behind an interface so the loop's logic can be tested without a window. */
internal interface FrameApi {
    fun windowShouldClose(): Boolean
    fun pollEvents()
    fun deltaTime(): Float
    fun renderFrame()
    fun endFrame()
    fun drawInstance(instance: Instance)
    fun drawModel(model: Model, transform: Transform)
}

internal object NativeFrameApi : FrameApi {
    override fun windowShouldClose() = exitRequested || NRE.windowShouldClose()
    override fun pollEvents() = NRE.pollEvents()
    override fun deltaTime() = NRE.getDeltaTime()
    override fun renderFrame() = NRE.renderFrame()
    override fun endFrame() = NRE.endFrame()
    override fun drawInstance(instance: Instance) = NRE.drawInstance(instance)
    override fun drawModel(model: Model, transform: Transform) = NRE.drawModel(model, transform)
}
