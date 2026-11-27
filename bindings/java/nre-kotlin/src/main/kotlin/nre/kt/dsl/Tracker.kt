package nre.kt.dsl

import nre.Cubemap
import nre.Handle
import nre.Instance
import nre.Light
import nre.Material
import nre.Model
import nre.NRE
import nre.Pipeline
import nre.Scene
import nre.Shader
import nre.Texture

/**
 * The resources a scope owns, in creation order, each with the function that destroys it. Not thread-safe (NRE is
 * single-threaded anyway). Handles are values, so a handle is "owned" when an equal one was tracked: a live NRE
 * handle is unique (its generation changes when the slot is reused).
 */
internal class Tracker {
    private val owned = LinkedHashMap<Handle, (Handle) -> Unit>()
    private var closed = false

    val size: Int get() = owned.size

    operator fun contains(handle: Handle): Boolean = handle in owned

    /** Throws once the scope has ended, so nothing is created after shutdown. */
    fun checkOpen() = check(!closed) { "this nre { } scope has ended: the renderer is shut down" }

    fun <H : Handle> track(handle: H, destroy: (H) -> Unit): H {
        checkOpen()
        require(!handle.isNone) { "can't track the empty handle $handle" }
        require(handle !in owned) { "$handle is already owned by this scope" }
        @Suppress("UNCHECKED_CAST")
        owned[handle] = destroy as (Handle) -> Unit
        return handle
    }

    /** Destroys [handle] now and forgets it, so the scope won't destroy it again. */
    fun destroy(handle: Handle) {
        val destroy = owned.remove(handle)
            ?: throw IllegalArgumentException(
                "$handle isn't owned by this scope: it was made with a raw NRE call (track it first) or already destroyed")
        destroy(handle)
    }

    /**
     * Destroys everything still owned, newest first, and closes the tracker. Keeps going when one fails and
     * returns the failures, in the order they happened.
     */
    fun destroyAll(): List<Throwable> {
        closed = true
        val errors = ArrayList<Throwable>()
        for ((handle, destroy) in owned.entries.toList().asReversed()) {
            owned.remove(handle)
            try {
                destroy(handle)
            } catch (t: Throwable) {
                errors += t
            }
        }
        return errors
    }
}

/** The NRE.destroyXxx that matches the handle's type. */
internal fun destroyHandle(handle: Handle) = when (handle) {
    is Shader -> NRE.destroyShader(handle)
    is Pipeline -> NRE.destroyPipeline(handle)
    is Model -> NRE.destroyModel(handle)
    is Instance -> NRE.destroyInstance(handle)
    is Light -> NRE.destroyLight(handle)
    is Scene -> NRE.destroyScene(handle)
    is Texture -> NRE.destroyTexture(handle)
    is Material -> NRE.destroyMaterial(handle)
    is Cubemap -> NRE.destroyCubemap(handle)
}

/**
 * Runs [block], then always destroys what [tracker] owns (newest first) and calls [shutdown], even when the block
 * or a destroy throws. The block's exception wins; cleanup failures are added to it as suppressed (or, after a
 * normal exit, the first one is thrown with the rest suppressed).
 */
internal fun runScope(tracker: Tracker, shutdown: () -> Unit, block: () -> Unit) {
    val failure = try {
        block()
        null
    } catch (t: Throwable) {
        t
    }
    val cleanup = tracker.destroyAll().toMutableList()
    try {
        shutdown()
    } catch (t: Throwable) {
        cleanup += t
    }
    val thrown = failure ?: cleanup.removeFirstOrNull() ?: return
    cleanup.forEach(thrown::addSuppressed)
    throw thrown
}
