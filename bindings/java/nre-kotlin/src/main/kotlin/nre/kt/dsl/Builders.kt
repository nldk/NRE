package nre.kt.dsl

import nre.Instance
import nre.Vec2
import nre.Vertex
import nre.NRE
import nre.Quat
import nre.Texture
import nre.Transform
import nre.Vec3
import nre.kt.normalized
import nre.kt.plus
import nre.kt.quat
import nre.kt.times
import java.nio.file.Path

/**
 * The block of `instance(...) { }`: the new instance's starting transform and visibility. The block runs first,
 * then the instance is created and set up with one call per changed part.
 */
@NreDsl
class InstanceBuilder internal constructor() {
    var position: Vec3 = Vec3.ZERO
    var rotation: Quat = Quat.IDENTITY
    var scale: Vec3 = Vec3.ONE
    var visible: Boolean = true

    var transform: Transform
        get() = Transform(position, rotation, scale)
        set(value) {
            position = value.position()
            rotation = value.rotation()
            scale = value.scale()
        }

    fun translate(offset: Vec3) {
        position += offset
    }

    /** Rotates by [angle] radians around the world-space [axis], like `Instance.rotate`. */
    fun rotate(angle: Float, axis: Vec3) {
        rotation = (quat(angle, axis) * rotation).normalized()
    }

    internal fun applyTo(instance: Instance) {
        val t = transform
        if (t != Transform.IDENTITY) NRE.setInstanceTransform(instance, t)
        if (!visible) NRE.setVisibility(instance, false)
    }
}

/**
 * The block of `material { }`. The textures are as glTF uses them; [Texture.NONE] (the default) means a white
 * base, not metal with roughness 1, and a flat normal. Textures loaded here are owned by the enclosing scope.
 *
 * The colour map is sRGB, the data maps ([metallic], [normal]) are not: `base(path)`, `metallic(path)` and
 * `normal(path)` load each the right way. [texture] is sRGB unless `srgb = false`, like renderer.h, so
 * `normal = texture(path)` would decode the normals as colours: write `normal(path)` or `texture(path, srgb = false)`.
 */
@NreDsl
class MaterialBuilder internal constructor(private val scope: NreScope) {
    var base: Texture = Texture.NONE

    /** Roughness in green, metallic in blue. */
    var metallic: Texture = Texture.NONE

    /** Tangent-space normal map, +Y up. */
    var normal: Texture = Texture.NONE

    /** Loads an image, sRGB unless [srgb] is false (see the class comment). */
    fun texture(path: Path, srgb: Boolean = true): Texture = scope.texture(path, srgb)

    /** Loads [path] as the colour map (sRGB). */
    fun base(path: Path) {
        base = texture(path, srgb = true)
    }

    /** Loads [path] as the metallic-roughness map (a data map: srgb = false). */
    fun metallic(path: Path) {
        metallic = texture(path, srgb = false)
    }

    /** Loads [path] as the normal map (a data map: srgb = false). */
    fun normal(path: Path) {
        normal = texture(path, srgb = false)
    }
}

/**
 * The block of `lighting(mode) { }` / `landscape { }`: what to change after the mode has reset the scene to its
 * defaults. Unset (null) parts keep the mode's defaults; the exposure isn't reset by the mode, so null keeps it.
 */
@NreDsl
class LightingBuilder internal constructor() {
    /** Metres from the camera that the sun casts shadows; 0 turns them off. */
    var shadowDistance: Float? = null

    /** Greater than 0; 2 is twice as bright. */
    var exposure: Float? = null

    /** The background without a skybox (Small), linear 0..1. */
    var skyColor: Vec3? = null

    private var haze: Pair<Float, Float>? = null

    /** Distance haze: [visibility] in metres (0 turns it off), [height] the layer's thickness. */
    fun haze(visibility: Float, height: Float = 300f) {
        haze = visibility to height
    }

    internal fun apply() {
        shadowDistance?.let(NRE::setShadowDistance)
        haze?.let { (visibility, height) -> NRE.setHaze(visibility, height) }
        exposure?.let(NRE::setExposure)
        skyColor?.let(NRE::setSkyColor)
    }
}

/**
 * The block of `mesh { }`: [vertex] adds a vertex and returns its index, [triangle] / [quad] connect them
 * (counter-clockwise seen from the front). Zero normals are computed from the triangles.
 */
@NreDsl
class MeshBuilder internal constructor() {
    internal val vertices = ArrayList<Vertex>()
    private var indices = IntArray(64)
    private var indexCount = 0

    val vertexCount: Int get() = vertices.size

    /** Adds a vertex; returns its index for [triangle]. */
    fun vertex(position: Vec3, normal: Vec3 = Vec3.ZERO, uv: Vec2 = Vec2.ZERO): Int {
        vertices += Vertex(position, normal, uv)
        return vertices.size - 1
    }

    fun triangle(a: Int, b: Int, c: Int) {
        if (indexCount + 3 > indices.size) indices = indices.copyOf(indices.size * 2)
        indices[indexCount] = a
        indices[indexCount + 1] = b
        indices[indexCount + 2] = c
        indexCount += 3
    }

    /** Two triangles, a-b-c and a-c-d: the corners in counter-clockwise order. */
    fun quad(a: Int, b: Int, c: Int, d: Int) {
        triangle(a, b, c)
        triangle(a, c, d)
    }

    internal fun indexArray(): IntArray = indices.copyOf(indexCount)
}
