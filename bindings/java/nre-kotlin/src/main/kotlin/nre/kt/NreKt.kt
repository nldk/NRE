// Kotlin sugar over the Java bindings in package nre: vector operators, destructuring, and a setRenderConfig with
// named, defaulted parameters. Everything else is used straight from nre.NRE. Import with `import nre.kt.*`.
package nre.kt

import nre.ApiMode
import nre.Instance
import nre.NRE
import nre.Quat
import nre.RayHit
import nre.RenderErrorPolicy
import nre.RenderMode
import nre.Vec2
import nre.Vec3

operator fun Vec2.plus(o: Vec2): Vec2 = add(o)
operator fun Vec2.minus(o: Vec2): Vec2 = sub(o)
operator fun Vec2.times(s: Float): Vec2 = scale(s)
operator fun Float.times(v: Vec2): Vec2 = v.scale(this)
operator fun Vec2.div(s: Float): Vec2 = scale(1f / s)
operator fun Vec2.unaryMinus(): Vec2 = Vec2(-x, -y)
operator fun Vec2.component1(): Float = x
operator fun Vec2.component2(): Float = y

operator fun Vec3.plus(o: Vec3): Vec3 = add(o)
operator fun Vec3.minus(o: Vec3): Vec3 = sub(o)
operator fun Vec3.times(s: Float): Vec3 = scale(s)
operator fun Float.times(v: Vec3): Vec3 = v.scale(this)
operator fun Vec3.div(s: Float): Vec3 = scale(1f / s)
operator fun Vec3.unaryMinus(): Vec3 = Vec3(-x, -y, -z)
operator fun Vec3.component1(): Float = x
operator fun Vec3.component2(): Float = y
operator fun Vec3.component3(): Float = z

operator fun Quat.component1(): Float = x
operator fun Quat.component2(): Float = y
operator fun Quat.component3(): Float = z
operator fun Quat.component4(): Float = w

/** `val (instance, distance, point) = pick(Mouse.position) ?: return` */
operator fun RayHit.component1(): Instance = instance()
operator fun RayHit.component2(): Float = distance()
operator fun RayHit.component3(): Vec3 = point()

/**
 * [NRE.setRenderConfig] with the C++ defaults as named parameters, so any of them can be set alone, e.g.
 * `setRenderConfig(800, 600, "demo", resizable = false)`. Must come before [NRE.init].
 */
fun setRenderConfig(
    width: Int,
    height: Int,
    title: String,
    mode: RenderMode = RenderMode.RENDER_3D,
    apiMode: ApiMode = ApiMode.SCENE,
    errorPolicy: RenderErrorPolicy = RenderErrorPolicy.RETURN,
    manualFramePresentationAndSubmission: Boolean = false,
    resizable: Boolean = true,
    showFpsInTitle: Boolean = true,
) = NRE.setRenderConfig(
    width, height, title, mode, apiMode, errorPolicy, manualFramePresentationAndSubmission, resizable, showFpsInTitle,
)
