// The camera and input as Kotlin objects and extension properties: `Camera.position = ...`, `Key.W.isDown`,
// `Mouse.delta`. Plain NRE calls, usable inside and outside nre { }. The camera is the bound scene's: in SceneMode,
// create a scene first (nre { } does that for you).
package nre.kt

import nre.CursorMode
import nre.Key
import nre.Mat4
import nre.MouseButton
import nre.NRE
import nre.RayHit
import nre.Vec2
import nre.Vec3

/** The bound scene's camera. Angles are in radians; yaw 0, pitch 0 looks along [Vec3.FORWARD] (-Z). */
object Camera {
    var position: Vec3
        get() = NRE.getCameraPosition()
        set(value) = NRE.setCameraPosition(value)

    /** The unit vector the camera looks along. */
    val forward: Vec3 get() = NRE.getCameraForward()

    val viewMatrix: Mat4 get() = NRE.getViewMatrix()

    /** Vulkan conventions: clip-space +Y down, reverse-Z with an infinite far plane (see renderer.h). */
    val projectionMatrix: Mat4 get() = NRE.getProjectionMatrix()

    fun lookAt(target: Vec3) = NRE.cameraLookAt(target)

    /** Moves in world space. */
    fun move(offset: Vec3) = NRE.moveCamera(offset)

    /** Moves along the camera's own axes: x right, y up, z forward (so +z is toward [forward], not -z). */
    fun moveRelative(offset: Vec3) = NRE.moveCameraRelative(offset)

    /** Positive yaw turns right, positive pitch looks up; pitch is clamped short of straight up/down. */
    fun rotate(yawDelta: Float, pitchDelta: Float) = NRE.rotateCamera(yawDelta, pitchDelta)

    /** nre.h has no getter for the angles, so this is a function, not a property. */
    fun setRotation(yaw: Float, pitch: Float) = NRE.setCameraRotation(yaw, pitch)
}

/** The mouse, as of the last pollEvents (which loop { } calls each frame). */
object Mouse {
    /** How far the cursor moved since the previous pollEvents, in pixels. +x is right, +y is down. */
    val delta: Vec2 get() = NRE.getMouseDelta()

    /** Pixels from the window's top-left corner. */
    val position: Vec2 get() = NRE.getMousePosition()

    /** Scroll wheel steps; positive is up / away from you. */
    val scroll: Float get() = NRE.getScrollDelta()

    /** nre.h can't read the mode back, so this is a function, not a property. */
    fun setCursorMode(mode: CursorMode) = NRE.setCursorMode(mode)
}

/**
 * The nearest visible instance of the bound scene under [windowPos] (pixels, like [Mouse.position]), or null:
 * `pick(Mouse.position)?.instance`. Render_3D only.
 */
fun pick(windowPos: Vec2): RayHit? = NRE.pickInstance(windowPos).orElse(null)

/** Held down now. */
val Key.isDown: Boolean get() = NRE.isKeyDown(this)

/** Went down during the last pollEvents: true for one frame. Holding doesn't repeat it. */
val Key.isPressed: Boolean get() = NRE.isKeyPressed(this)

/** Went up during the last pollEvents: true for one frame. */
val Key.isReleased: Boolean get() = NRE.isKeyReleased(this)

val MouseButton.isDown: Boolean get() = NRE.isMouseButtonDown(this)

val MouseButton.isPressed: Boolean get() = NRE.isMouseButtonPressed(this)

val MouseButton.isReleased: Boolean get() = NRE.isMouseButtonReleased(this)
