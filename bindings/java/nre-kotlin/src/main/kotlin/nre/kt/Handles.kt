// Extension properties and functions on the handles, so `car.position = vec3(0, 1, 0)` instead of
// NRE.setInstanceTransform(car, ...). Each one is a plain NRE call: they work with any handle, made by the DSL or not,
// inside or outside nre { }. Properties only where nre.h has both a getter and a setter; the rest are functions.
package nre.kt

import nre.Cubemap
import nre.Instance
import nre.Light
import nre.Mat4
import nre.Material
import nre.Model
import nre.NRE
import nre.Quat
import nre.Scene
import nre.SceneLighting
import nre.Texture
import nre.Transform
import nre.Vec3

// ---------------------------------------------------------------------------------------------------------------
// Instance. The part properties read the whole transform and write it back: two calls per set.
// ---------------------------------------------------------------------------------------------------------------

var Instance.transform: Transform
    get() = NRE.getInstanceTransform(this)
    set(value) = NRE.setInstanceTransform(this, value)

var Instance.position: Vec3
    get() = transform.position()
    set(value) { transform = transform.withPosition(value) }

var Instance.rotation: Quat
    get() = transform.rotation()
    set(value) { transform = transform.withRotation(value) }

var Instance.scale: Vec3
    get() = transform.scale()
    set(value) { transform = transform.withScale(value) }

/** The model matrix (translation * rotation * scale), column-major. */
val Instance.matrix: Mat4 get() = NRE.getInstanceMatrix(this)

fun Instance.translate(offset: Vec3) = NRE.translateInstance(this, offset)

/** Rotates by [angle] radians around the world-space [axis] (after the current rotation). */
fun Instance.rotate(angle: Float, axis: Vec3) = NRE.rotateInstance(this, angle, axis)

/** Multiplies the scale by [factor], per axis. */
fun Instance.scaleBy(factor: Vec3) = NRE.scaleInstance(this, factor)

/** A hidden instance stays in its scenes but isn't drawn. nre.h has no getter, so this is a function. */
fun Instance.setVisible(visible: Boolean) = NRE.setVisibility(this, visible)

/** Whether it casts sun shadows (it still receives them); default true, e.g. false for grass. A function: no getter. */
fun Instance.setCastsShadows(castsShadows: Boolean) = NRE.setCastsShadows(this, castsShadows)

/** Adds it to the bound scene (doing nothing if it's already there). */
fun Instance.addToScene() = NRE.addInstanceToScene(this)

fun Instance.removeFromScene() = NRE.removeInstanceFromScene(this)

// ---------------------------------------------------------------------------------------------------------------
// Model and Material
// ---------------------------------------------------------------------------------------------------------------

/** The model's material slots, in the file's order; empty for a model without materials (like OBJ). */
val Model.materials: List<Material> get() = NRE.getModelMaterials(this)

/** Makes every mesh use [material], replacing all slots. */
fun Model.setMaterial(material: Material) = NRE.setModelMaterial(this, material)

/** Replaces slot [index] of [materials], so only the meshes using that slot change. */
fun Model.replaceMaterial(index: Int, material: Material) = NRE.replaceModelMaterial(this, index, material)

/** [Texture.NONE] means white. Waits for the GPU, so not every frame. */
fun Material.setBaseTexture(texture: Texture) = NRE.setMaterialBaseTexture(this, texture)

// ---------------------------------------------------------------------------------------------------------------
// Light. nre.h has setters only, so these are functions.
// ---------------------------------------------------------------------------------------------------------------

/** Only used by point lights. */
fun Light.setPosition(position: Vec3) = NRE.setLightPosition(this, position)

/** The way the light travels. Only used by directional lights. */
fun Light.setDirection(direction: Vec3) = NRE.setLightDirection(this, direction)

fun Light.setColor(color: Vec3, intensity: Float) = NRE.setLightColor(this, color, intensity)

/** Adds it to the bound scene (doing nothing if it's already there). */
fun Light.addToScene() = NRE.addLightToScene(this)

fun Light.removeFromScene() = NRE.removeLightFromScene(this)

// ---------------------------------------------------------------------------------------------------------------
// Cubemap and Scene
// ---------------------------------------------------------------------------------------------------------------

/** Makes it the bound scene's skybox. `NRE.setSkybox(Cubemap.NONE)` removes the skybox again. */
fun Cubemap.useAsSkybox() = NRE.setSkybox(this)

fun Scene.bind() = NRE.bindScene(this)

/** Frames per second, averaged over the last half second. */
val fps: Float get() = NRE.getFPS()

/** The bound scene's sun direction (the way the light travels); setting it keeps the colour and intensity. */
var sunDirection: Vec3
    get() = NRE.getSunDirection()
    set(value) = NRE.setSunDirection(value)

/**
 * The bound scene's lighting: its mode, shadows, haze, exposure and sky colour. renderer.h's defaults: Small has
 * 20 m shadows and no haze, Landscape 500 m shadows and haze 8000 m / 300 m; exposure 1, sky (0.1, 0.3, 0.6).
 * Only [mode] can be read back, so the rest are functions.
 */
object Lighting {
    /** [SceneLighting.SMALL] (the default) or [SceneLighting.LANDSCAPE]. Setting it resets shadows, haze and sky. */
    var mode: SceneLighting
        get() = NRE.getSceneLighting()
        set(value) = NRE.setSceneLighting(value)

    /** How far from the camera the sun casts shadows, in metres; 0 turns them off. Must not be negative. */
    fun setShadowDistance(meters: Float) = NRE.setShadowDistance(meters)

    /** [visibility]: metres at which the haze hides almost everything, 0 = off. [height]: the layer's thickness. */
    fun setHaze(visibility: Float, height: Float = 300f) = NRE.setHaze(visibility, height)

    /** Greater than 0; 2 is twice as bright. */
    fun setExposure(exposure: Float) = NRE.setExposure(exposure)

    /** The background without a skybox (Small), linear 0..1. */
    fun setSkyColor(color: Vec3) = NRE.setSkyColor(color)
}
