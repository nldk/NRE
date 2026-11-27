// Small math helpers on top of the Java records: constructors that take any Number, degrees, quaternions from an
// axis and an angle, and the vector basics. The operators (+ - * /) are in NreKt.kt.
package nre.kt

import nre.Quat
import nre.Transform
import nre.Vec2
import nre.Vec3
import nre.Vertex
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.sin
import kotlin.math.sqrt

/** `vec3(0, 0, -1)` instead of `Vec3(0f, 0f, -1f)`. */
fun vec3(x: Number, y: Number, z: Number): Vec3 = Vec3(x.toFloat(), y.toFloat(), z.toFloat())

/** All three components the same, e.g. `scale = vec3(2)`. */
fun vec3(all: Number): Vec3 = all.toFloat().let { Vec3(it, it, it) }

fun vec2(x: Number, y: Number): Vec2 = Vec2(x.toFloat(), y.toFloat())

/** A [Vertex] for [nre.NRE.createMeshModel]; a zero [normal] (the default) is computed from the triangles. */
fun vertex(position: Vec3, normal: Vec3 = Vec3.ZERO, uv: Vec2 = Vec2.ZERO): Vertex = Vertex(position, normal, uv)

private const val DEG_TO_RAD = (PI / 180.0).toFloat()

/** Degrees to radians (NRE's angles are radians): `rotate(90.deg, Vec3.UP)`. */
val Float.deg: Float get() = this * DEG_TO_RAD
val Int.deg: Float get() = this * DEG_TO_RAD
val Double.deg: Float get() = (this * (PI / 180.0)).toFloat()

val Vec3.length: Float get() = sqrt(x * x + y * y + z * z)

/** This vector with length 1; the zero vector stays zero. */
fun Vec3.normalized(): Vec3 = length.let { if (it == 0f) this else Vec3(x / it, y / it, z / it) }

infix fun Vec3.dot(o: Vec3): Float = x * o.x + y * o.y + z * o.z

infix fun Vec3.cross(o: Vec3): Vec3 = Vec3(y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x)

/** The rotation by [angle] radians around [axis] (normalized here), like `glm::angleAxis`. */
fun quat(angle: Float, axis: Vec3): Quat {
    val a = axis.normalized()
    val s = sin(angle / 2)
    return Quat(a.x * s, a.y * s, a.z * s, cos(angle / 2))
}

/** Hamilton product, like glm: `a * b` rotates by b first, then by a. */
operator fun Quat.times(o: Quat): Quat = Quat(
    w * o.x + x * o.w + y * o.z - z * o.y,
    w * o.y - x * o.z + y * o.w + z * o.x,
    w * o.z + x * o.y - y * o.x + z * o.w,
    w * o.w - x * o.x - y * o.y - z * o.z,
)

/** Rotates [v] by this (unit) quaternion. */
operator fun Quat.times(v: Vec3): Vec3 {
    val u = Vec3(x, y, z)
    val t = (u cross v) * 2f
    return v + t * w + (u cross t)
}

fun Quat.normalized(): Quat {
    val n = sqrt(x * x + y * y + z * z + w * w)
    return if (n == 0f) Quat.IDENTITY else Quat(x / n, y / n, z / n, w / n)
}

/** A [Transform] with named, defaulted parts: `transform(position = vec3(0, 1, 0))`. */
fun transform(position: Vec3 = Vec3.ZERO, rotation: Quat = Quat.IDENTITY, scale: Vec3 = Vec3.ONE): Transform =
    Transform(position, rotation, scale)

/** This transform rotated by [angle] radians around the world-space [axis], like [nre.NRE.rotateInstance]. */
fun Transform.rotated(angle: Float, axis: Vec3): Transform = withRotation((quat(angle, axis) * rotation()).normalized())

fun Transform.translated(offset: Vec3): Transform = withPosition(position() + offset)

operator fun Transform.component1(): Vec3 = position()
operator fun Transform.component2(): Quat = rotation()
operator fun Transform.component3(): Vec3 = scale()
