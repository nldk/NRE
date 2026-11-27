package nre.kt.dsl

import nre.ApiMode
import nre.CursorMode
import nre.Key
import nre.MouseButton
import nre.SceneLighting
import nre.Vec3
import nre.kt.*
import org.junit.jupiter.api.Assertions.assertNotNull
import org.junit.jupiter.api.Test
import kotlin.io.path.Path
import kotlin.io.path.div

/**
 * Compile-time checks: these blocks are type-checked but never run (running them needs a window). What must NOT
 * compile is listed at the bottom; @DslMarker is what rejects it.
 */
class DslShapeTest {
    @Test
    fun theDslShapesCompile() {
        val assets = Path("assets")
        val app: NreScope.() -> Unit = {
            pipeline(vertex = assets / "v.spv", fragment = assets / "f.spv")
            skybox(assets / "sky")
            sun(direction = vec3(-1, -1, 0))
            val car = instance(assets / "car.glb") {
                position = vec3(0, 0, -1)
                rotate(90.deg, Vec3.UP)
                scale = vec3(2)
                visible = true
            }
            val model = model(assets / "box.glb")
            val box = model.instance(addToScene = false) { translate(vec3(1, 0, 0)) }
            val mat = material {
                base = texture(assets / "base.png")
                normal(assets / "normal.png")                      // a data map: srgb = false
                metallic = texture(assets / "orm.png", srgb = false)
            }
            model.setMaterial(mat)
            lighting = SceneLighting.LANDSCAPE
            landscape {
                exposure = 1.2f
                shadowDistance = 200f
                haze(5000f, height = 200f)
            }
            lighting(SceneLighting.SMALL) { skyColor = vec3(0.2, 0.3, 0.5) }
            val ground = mesh(material = mat) {
                val a = vertex(vec3(-1, 0, 1), uv = vec2(0, 0))
                val b = vertex(vec3(1, 0, 1), uv = vec2(1, 0))
                val c = vertex(vec3(1, 0, -1), normal = Vec3.UP)
                val d = vertex(vec3(-1, 0, -1))
                quad(a, b, c, d)
                triangle(a, b, c)
            }
            val grass = ground.instance { position = vec3(0, -1, 0) }
            grass.setCastsShadows(false)
            meshModel(listOf(vertex(Vec3.ZERO), vertex(Vec3.RIGHT), vertex(Vec3.UP)), intArrayOf(0, 1, 2))
            Lighting.mode = SceneLighting.LANDSCAPE
            Lighting.setHaze(8000f)
            Lighting.setExposure(1f)
            Lighting.setShadowDistance(0f)
            Lighting.setSkyColor(Vec3.ONE)
            val projection: nre.Mat4 = Camera.projectionMatrix
            assertNotNull(projection)
            val lamp = pointLight(position = vec3(0, 3, 0), intensity = 20f)
            lamp.setColor(Vec3.ONE, 15f)
            directionalLight(direction = vec3(0, -1, 0), addToScene = false)
            loop { dt ->
                car.rotate(dt, Vec3.UP)
                car.position += vec3(0, dt, 0)
                Camera.lookAt(car.position)
                if (MouseButton.LEFT.isDown) Camera.rotate(Mouse.delta.x, Mouse.delta.y)
                if (Key.SPACE.isPressed) Mouse.setCursorMode(CursorMode.LOCKED)
                if (Key.ESCAPE.isPressed) close()
                if (MouseButton.LEFT.isPressed) pick()?.let { (instance, distance, point) ->
                    if (distance < 10f && point.y > 0f) instance.setVisible(false)
                }
                val clicked: nre.Instance? = pick(Mouse.position)?.instance()
                if (clicked == car) lighting = SceneLighting.SMALL
                if (apiMode == ApiMode.FUNCTION_DRAW) {
                    render {
                        draw(car)
                        draw(model, position = vec3(1, 0, 0))
                        draw(model, transform(scale = vec3(0.5)))
                        if (Key.Q.isDown) close()   // LoopScope isn't marked, so close() is reachable
                    }
                }
                destroy(box)   // NreScope functions are usable inside loop { }
            }
        }
        assertNotNull(app)

        // These don't compile (the @DslMarker blocks the outer receiver):
        //   instance(model) { instance(model) }        // instance() inside an instance builder
        //   material { pointLight(Vec3.ZERO) }         // scope functions inside material { }
        //   loop { render { instance(model) } }        // creating resources inside the frame
        //   mesh { pointLight(Vec3.ZERO) }             // scope functions inside mesh { }
        //   landscape { lighting = SceneLighting.SMALL } // the scope's lighting inside its own builder
        // And draw(...) only exists inside render { }: `loop { draw(car) }` doesn't compile.
    }
}
