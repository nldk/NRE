package nre.kt

import nre.Quat
import nre.Transform
import nre.Vec3
import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Test
import kotlin.math.PI
import kotlin.math.abs

class MathTest {
    private fun assertNear(expected: Vec3, actual: Vec3, eps: Float = 1e-5f) {
        val ok = abs(expected.x - actual.x) < eps && abs(expected.y - actual.y) < eps && abs(expected.z - actual.z) < eps
        if (!ok) throw AssertionError("expected $expected, got $actual")
    }

    @Test
    fun degreesToRadians() {
        assertEquals(PI.toFloat(), 180.deg, 1e-6f)
        assertEquals((PI / 2).toFloat(), 90f.deg, 1e-6f)
        assertEquals((PI / 4).toFloat(), 45.0.deg, 1e-6f)
        assertEquals(0f, 0.deg)
    }

    @Test
    fun vectorsFromAnyNumber() {
        assertEquals(Vec3(1f, 2.5f, -3f), vec3(1, 2.5, -3L))
        assertEquals(Vec3(2f, 2f, 2f), vec3(2))
        assertEquals(nre.Vec2(1f, 0.5f), vec2(1, 0.5f))
    }

    @Test
    fun directionConstants() {
        assertEquals(Vec3(0f, 1f, 0f), Vec3.UP)
        assertEquals(Vec3(1f, 0f, 0f), Vec3.RIGHT)
        assertEquals(Vec3(0f, 0f, -1f), Vec3.FORWARD)
        // Right-handed: right × up is backward (+Z), the opposite of FORWARD.
        assertEquals(Vec3(0f, 0f, 1f), Vec3.RIGHT cross Vec3.UP)
    }

    @Test
    fun vectorBasics() {
        assertEquals(5f, vec3(3, 4, 0).length)
        assertNear(vec3(0.6, 0.8, 0), vec3(3, 4, 0).normalized())
        assertEquals(Vec3.ZERO, Vec3.ZERO.normalized())
        assertEquals(11f, vec3(1, 2, 3) dot vec3(3, 1, 2))
    }

    @Test
    fun axisAngleQuaternions() {
        // A quarter turn around +Y takes +X to -Z (right-handed, like glm::angleAxis).
        assertNear(Vec3.FORWARD, quat(90.deg, Vec3.UP) * Vec3.RIGHT)
        // The axis is normalized.
        assertNear(Vec3.FORWARD, quat(90.deg, vec3(0, 5, 0)) * Vec3.RIGHT)
        // a * b rotates by b first: +X -(Y 90°)-> -Z -(X 90°)-> +Y.
        val combined = quat(90.deg, Vec3.RIGHT) * quat(90.deg, Vec3.UP)
        assertNear(Vec3.UP, combined * Vec3.RIGHT)
        assertEquals(Quat.IDENTITY, quat(0f, Vec3.UP))
        val q = Quat(0f, 2f, 0f, 0f).normalized()
        assertEquals(Quat(0f, 1f, 0f, 0f), q)
    }

    @Test
    fun transformHelpers() {
        val t = transform(position = vec3(1, 0, 0))
        assertEquals(Transform(vec3(1, 0, 0), Quat.IDENTITY, Vec3.ONE), t)
        assertEquals(vec3(1, 2, 0), t.translated(vec3(0, 2, 0)).position())
        assertNear(Vec3.FORWARD, t.rotated(90.deg, Vec3.UP).rotation() * Vec3.RIGHT)
        val (p, r, s) = t
        assertEquals(Transform(p, r, s), t)
    }
}
