package nre.kt.dsl

import nre.ApiMode
import nre.Instance
import nre.Model
import nre.RenderMode
import nre.Texture
import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Assertions.assertFalse
import org.junit.jupiter.api.Assertions.assertSame
import org.junit.jupiter.api.Assertions.assertTrue
import org.junit.jupiter.api.Test
import org.junit.jupiter.api.assertThrows

/** The scope's ownership and cleanup, with fake destroy functions: no library, no window. */
class ScopeTest {
    private val log = mutableListOf<String>()
    private fun destroyer(name: String): (Any) -> Unit = { log += "destroy $name" }

    @Test
    fun destroysInReverseCreationOrder() {
        val t = Tracker()
        t.track(Model(1, 1), destroyer("model"))
        t.track(Instance(1, 1), destroyer("instance 1"))
        t.track(Instance(2, 1), destroyer("instance 2"))
        t.track(Texture(1, 1), destroyer("texture"))
        assertEquals(emptyList<Throwable>(), t.destroyAll())
        assertEquals(listOf("destroy texture", "destroy instance 2", "destroy instance 1", "destroy model"), log)
        assertEquals(0, t.size)
    }

    @Test
    fun handlesOfDifferentTypesWithTheSameNumbersAreDifferent() {
        val t = Tracker()
        t.track(Model(1, 1), destroyer("model"))
        t.track(Instance(1, 1), destroyer("instance"))
        assertEquals(2, t.size)
    }

    @Test
    fun destroyEarlyStopsOwningSoItIsNotDestroyedTwice() {
        val t = Tracker()
        val a = t.track(Instance(1, 1), destroyer("a"))
        t.track(Instance(2, 1), destroyer("b"))
        t.destroy(a)
        assertEquals(listOf("destroy a"), log)
        assertFalse(a in t)
        assertThrows<IllegalArgumentException> { t.destroy(a) }
        t.destroyAll()
        assertEquals(listOf("destroy a", "destroy b"), log)
    }

    @Test
    fun rejectsUnownedEmptyAndDuplicateHandles() {
        val t = Tracker()
        assertThrows<IllegalArgumentException> { t.destroy(Instance(5, 5)) }
        assertThrows<IllegalArgumentException> { t.track(Instance.NONE, destroyer("none")) }
        t.track(Instance(1, 1), destroyer("a"))
        assertThrows<IllegalArgumentException> { t.track(Instance(1, 1), destroyer("again")) }
    }

    @Test
    fun nothingCanBeTrackedAfterTheScopeEnded() {
        val t = Tracker()
        t.destroyAll()
        assertThrows<IllegalStateException> { t.checkOpen() }
        assertThrows<IllegalStateException> { t.track(Instance(1, 1), destroyer("late")) }
    }

    @Test
    fun keepsDestroyingWhenOneFails() {
        val t = Tracker()
        t.track(Instance(1, 1), destroyer("a"))
        t.track(Instance(2, 1)) { throw IllegalStateException("b failed") }
        t.track(Instance(3, 1), destroyer("c"))
        val errors = t.destroyAll()
        assertEquals(listOf("destroy c", "destroy a"), log)
        assertEquals(listOf("b failed"), errors.map { it.message })
    }

    @Test
    fun normalExitDestroysThenShutsDown() {
        val t = Tracker()
        runScope(t, shutdown = { log += "shutdown" }) {
            t.track(Model(1, 1), destroyer("model"))
            t.track(Instance(1, 1), destroyer("instance"))
            log += "block"
        }
        assertEquals(listOf("block", "destroy instance", "destroy model", "shutdown"), log)
    }

    @Test
    fun exceptionStillDestroysAndShutsDownAndWins() {
        val t = Tracker()
        val boom = IllegalStateException("boom")
        val thrown = assertThrows<IllegalStateException> {
            runScope(t, shutdown = { log += "shutdown"; throw RuntimeException("shutdown failed") }) {
                t.track(Model(1, 1), destroyer("model"))
                t.track(Instance(1, 1)) { throw RuntimeException("destroy failed") }
                throw boom
            }
        }
        assertSame(boom, thrown)
        assertEquals(listOf("destroy model", "shutdown"), log)
        assertEquals(listOf("destroy failed", "shutdown failed"), thrown.suppressed.map { it.message })
    }

    @Test
    fun cleanupFailureAfterNormalExitIsThrown() {
        val t = Tracker()
        val thrown = assertThrows<RuntimeException> {
            runScope(t, shutdown = { log += "shutdown" }) {
                t.track(Model(1, 1)) { throw RuntimeException("first") }
                t.track(Instance(1, 1)) { throw RuntimeException("second") }
            }
        }
        assertEquals("second", thrown.message)   // newest is destroyed first
        assertEquals(listOf("first"), thrown.suppressed.map { it.message })
        assertEquals(listOf("shutdown"), log)
    }

    @Test
    fun loopRejectsManualPresentation() {
        val scope = NreScope(RenderMode.RENDER_3D, ApiMode.SCENE, manualFramePresentationAndSubmission = true)
        val e = assertThrows<IllegalStateException> { scope.loop { } }
        assertTrue("manualFramePresentationAndSubmission" in e.message!!)
    }

    @Test
    fun scopeDestroyRejectsHandlesItDoesNotOwn() {
        val scope = NreScope(RenderMode.RENDER_3D, ApiMode.SCENE, false)
        assertThrows<IllegalArgumentException> { scope.destroy(Instance(1, 1)) }
    }
}
