package nre.kt.dsl

import nre.ApiMode
import nre.Instance
import nre.Model
import nre.Transform
import nre.kt.vec3
import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Assertions.assertTrue
import org.junit.jupiter.api.Test
import org.junit.jupiter.api.assertThrows

/** The frame loop's call order, against a fake FrameApi that records what the loop calls. */
class LoopTest {
    private class FakeFrames(private val framesUntilClose: Int = Int.MAX_VALUE) : FrameApi {
        val calls = mutableListOf<String>()
        var failRenderFrame = false
        private var polls = 0

        override fun windowShouldClose() = polls >= framesUntilClose
        override fun pollEvents() { polls++; calls += "poll" }
        override fun deltaTime() = polls * 0.5f
        override fun renderFrame() {
            calls += "renderFrame"
            if (failRenderFrame) throw IllegalStateException("renderFrame failed")
        }
        override fun endFrame() { calls += "endFrame" }
        override fun drawInstance(instance: Instance) { calls += "drawInstance ${instance.index()}" }
        override fun drawModel(model: Model, transform: Transform) {
            calls += "drawModel ${model.index()} at ${transform.position()}"
        }
    }

    private fun loop(mode: ApiMode, frames: FakeFrames, block: LoopScope.(Float) -> Unit) =
        LoopScope(mode, frames).run(block)

    @Test
    fun sceneModeRunsTheBlockThenRenders() {
        val f = FakeFrames(framesUntilClose = 2)
        val dts = mutableListOf<Float>()
        loop(ApiMode.SCENE, f) { dt -> dts += dt; f.calls += "block" }
        assertEquals(listOf("poll", "block", "renderFrame", "poll", "block", "renderFrame"), f.calls)
        assertEquals(listOf(0.5f, 1.0f), dts)
    }

    @Test
    fun drawModeDrawsBetweenRenderFrameAndEndFrame() {
        val f = FakeFrames(framesUntilClose = 1)
        loop(ApiMode.FUNCTION_DRAW, f) {
            f.calls += "update"
            render {
                draw(Instance(7, 1))
                draw(Model(3, 1), position = vec3(1, 2, 3))
            }
            f.calls += "after"
        }
        assertEquals(
            listOf(
                "poll", "update", "renderFrame", "drawInstance 7", "drawModel 3 at Vec3[x=1.0, y=2.0, z=3.0]",
                "endFrame", "after",
            ),
            f.calls,
        )
    }

    @Test
    fun drawModeWithoutRenderGetsAnEmptyFrame() {
        val f = FakeFrames(framesUntilClose = 1)
        loop(ApiMode.FUNCTION_DRAW, f) { }
        assertEquals(listOf("poll", "renderFrame", "endFrame"), f.calls)
    }

    @Test
    fun closeEndsTheLoopWithoutRenderingThatIteration() {
        val f = FakeFrames()
        var n = 0
        loop(ApiMode.SCENE, f) { if (++n == 3) close() }
        assertEquals(3, n)
        assertEquals(listOf("poll", "renderFrame", "poll", "renderFrame", "poll"), f.calls)
    }

    @Test
    fun drawingIsRejectedInSceneMode() {
        val e = assertThrows<IllegalStateException> {
            loop(ApiMode.SCENE, FakeFrames(1)) { render { } }
        }
        assertTrue("FUNCTION_DRAW" in e.message!!)
    }

    @Test
    fun onlyOneFramePerIteration() {
        assertThrows<IllegalStateException> {
            loop(ApiMode.SCENE, FakeFrames(1)) { render(); render() }
        }
    }

    @Test
    fun exceptionInRenderStillEndsTheFrame() {
        val f = FakeFrames()
        val e = assertThrows<IllegalStateException> {
            loop(ApiMode.FUNCTION_DRAW, f) { render { draw(Instance(1, 1)); error("boom") } }
        }
        assertEquals("boom", e.message)
        assertEquals(listOf("poll", "renderFrame", "drawInstance 1", "endFrame"), f.calls)
    }

    @Test
    fun failedRenderFrameStillEndsTheFrame() {
        val f = FakeFrames().apply { failRenderFrame = true }
        val drew = mutableListOf<Boolean>()
        assertThrows<IllegalStateException> {
            loop(ApiMode.FUNCTION_DRAW, f) { render { drew += true } }
        }
        assertEquals(emptyList<Boolean>(), drew)
        assertEquals(listOf("poll", "renderFrame", "endFrame"), f.calls)
    }

    @Test
    fun drawScopeAndLoopScopeCanNotBeUsedLater() {
        var leakedDraw: DrawScope? = null
        var leakedLoop: LoopScope? = null
        loop(ApiMode.FUNCTION_DRAW, FakeFrames(1)) {
            leakedLoop = this
            render { leakedDraw = this }
        }
        assertThrows<IllegalStateException> { leakedDraw!!.draw(Instance(1, 1)) }
        assertThrows<IllegalStateException> { leakedLoop!!.render() }
    }
}
