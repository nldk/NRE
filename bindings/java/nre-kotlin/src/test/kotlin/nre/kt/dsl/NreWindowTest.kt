package nre.kt.dsl

import nre.ApiMode
import nre.ErrorCode
import nre.NreException
import nre.RayHit
import nre.SceneLighting
import nre.Vec3
import nre.kt.*
import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Assertions.assertNull
import org.junit.jupiter.api.Assertions.assertTrue
import org.junit.jupiter.api.Test
import org.junit.jupiter.api.assertThrows
import org.junit.jupiter.api.condition.EnabledIfEnvironmentVariable
import kotlin.io.path.Path
import kotlin.io.path.div

/**
 * Opens a window: only runs with NRE_WINDOW_TESTS=1 (and NRE_LIB_DIR, and the example shaders built), e.g.
 * `NRE_WINDOW_TESTS=1 ./gradlew :nre-kotlin:test --tests '*NreWindowTest'`.
 */
@EnabledIfEnvironmentVariable(named = "NRE_WINDOW_TESTS", matches = "1")
class NreWindowTest {
    private val repo = Path(System.getProperty("nre.repo"))
    private val assets = repo / "example/assets"
    private val shaders = System.getenv("NRE_SHADER_DIR")?.takeIf { it.isNotEmpty() }?.let { Path(it) }
        ?: (repo / "cmake-build-debug/example/shaders")

    private fun NreScope.demo(): nre.Instance {
        pipeline(vertex = shaders / "vertex.vert.spv", fragment = shaders / "fragment.frag.spv")
        skybox(assets / "skyboxes/plains")
        sun(direction = vec3(-1, -1, 0))
        val box = model(assets / "BoxTextured.glb")
        box.setMaterial(material { base = texture(assets / "Textures/colormap.png") })
        box.instance { position = vec3(1.5, 0, -1) }
        pointLight(position = vec3(0, 2, 0))
        return instance(assets / "police.glb") { position = vec3(0, 0, -1) }
    }

    @Test
    fun scopeCleansUpOnExceptionAndOnNormalExit() {
        // Exception in the frame: endFrame, destroys and shutdown still run (else the second init below fails).
        val e = assertThrows<IllegalStateException> {
            nre(320, 240, "NreWindowTest 1", apiMode = ApiMode.FUNCTION_DRAW) {
                val car = demo()
                var frames = 0
                loop { if (++frames == 10) render { draw(car); error("boom") } }
            }
        }
        assertEquals("boom", e.message)

        var frames = 0
        nre(320, 240, "NreWindowTest 2", apiMode = ApiMode.SCENE) {
            val car = demo()
            assertTrue(car.position == vec3(0, 0, -1))
            Camera.position = vec3(0, 1, 2)
            Camera.lookAt(car.position)
            loop { dt ->
                car.rotate(dt, Vec3.UP)
                if (++frames == 30) close()
            }
        }
        assertEquals(30, frames)
    }

    /** The 0.3 additions through the DSL: lighting, shadows, haze, exposure, mesh models, projection, picking. */
    @Test
    fun lightingMeshesAndPicking() {
        var hit: RayHit? = null
        var miss: RayHit? = null
        nre(400, 400, "NreWindowTest 3") {
            pipeline(vertex = shaders / "vertex.vert.spv", fragment = shaders / "fragment.frag.spv")
            texture(assets / "skyboxes/plains/top.png", srgb = false)
            landscape {
                shadowDistance = 100f
                haze(5000f, height = 200f)
                exposure = 1.5f
                skyColor = vec3(0.2, 0.3, 0.5)
            }
            assertEquals(SceneLighting.LANDSCAPE, lighting)
            sun(direction = vec3(-1, -1, 0))
            val box = instance(assets / "Box.glb")
            box.setCastsShadows(false)
            val e = assertThrows<NreException> { Lighting.setShadowDistance(-1f) }
            assertEquals(ErrorCode.INVALID_ARGUMENT, e.code())
            val ground = mesh(material { base(assets / "Textures/colormap.png") }) {
                val a = vertex(vec3(-5, 0, 5))
                val b = vertex(vec3(5, 0, 5))
                val c = vertex(vec3(5, 0, -5))
                val d = vertex(vec3(-5, 0, -5))
                quad(a, b, c, d)
            }
            ground.instance { position = vec3(0, -2, 0) }
            Camera.position = vec3(0, 0, 3)
            Camera.lookAt(Vec3.ZERO)
            var frames = 0
            loop {
                if (++frames == 30) {
                    hit = pick(vec2(200, 200))
                    miss = pick(vec2(2, 2))
                    close()
                }
            }
            val m = Camera.projectionMatrix.toArray()
            assertEquals(-1.732f, m[5], 1e-3f)
            assertEquals(0.1f, m[14], 1e-4f)
            val h = hit ?: error("pick(200, 200) hit nothing")
            assertEquals(box, h.instance())
            assertEquals(2.5f, h.distance(), 1e-3f)
            assertTrue((h.point() - vec3(0, 0, 0.5)).length < 1e-3f)
            assertNull(miss)
        }
    }
}
