package nre

/*
#include <stdlib.h>
#include "nre.h"
*/
import "C"

import (
	"fmt"
	"math"
	"unsafe"
)

// The functions of nre.h, in its order. See renderer.h for what each one does. GLFW needs them called
// from the main OS thread (see the package doc).

// cString returns s as a C string; free it with C.free.
func cString(s string) *C.char { return C.CString(s) }

func free(p *C.char) { C.free(unsafe.Pointer(p)) }

// ---------------------------------------------------------------------------
// Setup and frames
// ---------------------------------------------------------------------------

// SetRenderConfig sets the window size and title, with renderer.h's usual defaults for the rest:
// Render3D, SceneMode, ErrorPolicyReturn, automatic presentation and submission, resizable, FPS in the
// title. Call it before Init. SetRenderConfigFull takes every setting.
func SetRenderConfig(windowWidth, windowHeight int, windowTitle string) error {
	return SetRenderConfigFull(windowWidth, windowHeight, windowTitle,
		Render3D, SceneMode, ErrorPolicyReturn, false, true, true)
}

// SetRenderConfigFull is SetRenderConfig with every setting (renderer.h's RenderConfig and WindowConfig).
func SetRenderConfigFull(windowWidth, windowHeight int, windowTitle string,
	mode RenderMode, apiMode ApiMode, errorPolicy RenderErrorPolicy,
	manualFramePresentationAndSubmission, resizable, showFpsInTitle bool) error {
	if err := checkVersion(); err != nil {
		return err
	}
	title := cString(windowTitle)
	defer free(title)
	return call(func() C.NreErrorCode {
		return C.nreSetRenderConfig(C.NreRenderMode(mode), C.NreApiMode(apiMode),
			C.NreRenderErrorPolicy(errorPolicy), C.bool(manualFramePresentationAndSubmission),
			C.int(windowWidth), C.int(windowHeight), title, C.bool(resizable), C.bool(showFpsInTitle))
	})
}

// Init opens the window and sets up Vulkan. It first checks that libnre_c is a 0.3.x version.
func Init() error {
	if err := checkVersion(); err != nil {
		return err
	}
	return call(func() C.NreErrorCode { return C.nreInit() })
}

// Shutdown destroys everything and closes the window.
func Shutdown() error { return call(func() C.NreErrorCode { return C.nreShutdown() }) }

// RenderFrame draws the bound scene (SceneMode) or starts a frame (FunctionDrawMode). See renderer.h.
func RenderFrame() error { return call(func() C.NreErrorCode { return C.nreRenderFrame() }) }

// EndFrame ends the frame RenderFrame started (FunctionDrawMode only).
func EndFrame() error { return call(func() C.NreErrorCode { return C.nreEndFrame() }) }

// PresentFrame presents the frame (with manualFramePresentationAndSubmission).
func PresentFrame() error { return call(func() C.NreErrorCode { return C.nrePresentFrame() }) }

// SubmitFrame submits the frame (with manualFramePresentationAndSubmission).
func SubmitFrame() error { return call(func() C.NreErrorCode { return C.nreSubmitFrame() }) }

// WindowShouldClose reports whether the user asked to close the window.
func WindowShouldClose() (bool, error) {
	var v C.bool
	err := call(func() C.NreErrorCode { return C.nreWindowShouldClose(&v) })
	return bool(v), err
}

// PollEvents processes window and input events; call it once per frame.
func PollEvents() error { return call(func() C.NreErrorCode { return C.nrePollEvents() }) }

// ---------------------------------------------------------------------------
// Shaders and pipelines
// ---------------------------------------------------------------------------

// CreateShader loads a SPIR-V file.
func CreateShader(shaderPath string, shaderType ShaderType) (Shader, error) {
	path := cString(shaderPath)
	defer free(path)
	var h C.NreShader
	err := call(func() C.NreErrorCode { return C.nreCreateShader(path, C.NreShaderType(shaderType), &h) })
	return shaderFromC(h), err
}

func DestroyShader(shader Shader) error {
	return call(func() C.NreErrorCode { return C.nreDestroyShader(shader.c()) })
}

func CreatePipeline(vertexShader, fragmentShader Shader) (Pipeline, error) {
	var h C.NrePipeline
	err := call(func() C.NreErrorCode { return C.nreCreatePipeline(vertexShader.c(), fragmentShader.c(), &h) })
	return pipelineFromC(h), err
}

func DestroyPipeline(pipeline Pipeline) error {
	return call(func() C.NreErrorCode { return C.nreDestroyPipeline(pipeline.c()) })
}

func BindPipeline(pipeline Pipeline) error {
	return call(func() C.NreErrorCode { return C.nreBindPipeline(pipeline.c()) })
}

// ---------------------------------------------------------------------------
// Models and instances
// ---------------------------------------------------------------------------

// CreateModel loads a model file (glTF/GLB, OBJ, ...). It isn't drawn by itself: see CreateInstance and DrawModel.
func CreateModel(modelPath string) (Model, error) {
	path := cString(modelPath)
	defer free(path)
	var h C.NreModel
	err := call(func() C.NreErrorCode { return C.nreCreateModel(path, &h) })
	return modelFromC(h), err
}

// CreateMeshModel makes a model from your own triangles (terrain, water, editor meshes): a triangle list,
// counter-clockwise front faces, indices into vertices. Vertices with a zero Normal get one computed from the
// triangles that use them. material Material{} uses the default material. The slices are handed to C without
// copying (Vertex has NreVertex's layout) and the renderer copies the geometry, so they can be reused after.
func CreateMeshModel(vertices []Vertex, indices []uint32, material Material) (Model, error) {
	if uint64(len(vertices)) > math.MaxUint32 || uint64(len(indices)) > math.MaxUint32 {
		return Model{}, &Error{Code: ErrInvalidArgument,
			Message: fmt.Sprintf("CreateMeshModel: %d vertices / %d indices don't fit in uint32", len(vertices), len(indices))}
	}
	var v *C.NreVertex
	if len(vertices) > 0 {
		v = (*C.NreVertex)(unsafe.Pointer(&vertices[0]))
	}
	var i *C.uint32_t
	if len(indices) > 0 {
		i = (*C.uint32_t)(unsafe.Pointer(&indices[0]))
	}
	var h C.NreModel
	err := call(func() C.NreErrorCode {
		return C.nreCreateMeshModel(v, C.uint32_t(len(vertices)), i, C.uint32_t(len(indices)), material.c(), &h)
	})
	return modelFromC(h), err
}

// DestroyModel destroys the model. Its instances stay valid but draw nothing.
func DestroyModel(model Model) error {
	return call(func() C.NreErrorCode { return C.nreDestroyModel(model.c()) })
}

// CreateInstance makes a copy of the model with its own transform (identity at first).
func CreateInstance(model Model) (Instance, error) {
	var h C.NreInstance
	err := call(func() C.NreErrorCode { return C.nreCreateInstance(model.c(), &h) })
	return instanceFromC(h), err
}

// DestroyInstance destroys the instance and removes it from every scene it's in.
func DestroyInstance(instance Instance) error {
	return call(func() C.NreErrorCode { return C.nreDestroyInstance(instance.c()) })
}

func SetInstanceTransform(instance Instance, transform Transform) error {
	t := transform.c()
	return call(func() C.NreErrorCode { return C.nreSetInstanceTransform(instance.c(), &t) })
}

func GetInstanceTransform(instance Instance) (Transform, error) {
	var t C.NreTransform
	err := call(func() C.NreErrorCode { return C.nreGetInstanceTransform(instance.c(), &t) })
	return transformFromC(t), err
}

func GetInstanceMatrix(instance Instance) (Mat4, error) {
	var m C.NreMat4
	err := call(func() C.NreErrorCode { return C.nreGetInstanceMatrix(instance.c(), &m) })
	return mat4FromC(m), err
}

func TranslateInstance(instance Instance, offset Vec3) error {
	return call(func() C.NreErrorCode { return C.nreTranslateInstance(instance.c(), offset.c()) })
}

// RotateInstance rotates by angle (radians) around axis.
func RotateInstance(instance Instance, angle float32, axis Vec3) error {
	return call(func() C.NreErrorCode { return C.nreRotateInstance(instance.c(), C.float(angle), axis.c()) })
}

func ScaleInstance(instance Instance, factor Vec3) error {
	return call(func() C.NreErrorCode { return C.nreScaleInstance(instance.c(), factor.c()) })
}

// SetVisibility hides or shows the instance. A hidden instance stays in its scenes but isn't drawn.
func SetVisibility(instance Instance, visible bool) error {
	return call(func() C.NreErrorCode { return C.nreSetVisibility(instance.c(), C.bool(visible)) })
}

// SetCastsShadows sets whether the instance casts sun shadows (it still receives them). Default true; e.g.
// off for grass, for speed. DrawModel always casts.
func SetCastsShadows(instance Instance, castsShadows bool) error {
	return call(func() C.NreErrorCode { return C.nreSetCastsShadows(instance.c(), C.bool(castsShadows)) })
}

// DrawModel draws the model once with the identity transform: FunctionDrawMode only, between RenderFrame
// and EndFrame. DrawModelFull takes a transform.
func DrawModel(model Model) error {
	return call(func() C.NreErrorCode { return C.nreDrawModel(model.c(), nil) })
}

// DrawModelFull draws the model once with this transform (FunctionDrawMode, between RenderFrame and EndFrame).
func DrawModelFull(model Model, transform Transform) error {
	t := transform.c()
	return call(func() C.NreErrorCode { return C.nreDrawModel(model.c(), &t) })
}

// DrawInstance draws the instance with its transform (FunctionDrawMode). A hidden instance isn't drawn.
func DrawInstance(instance Instance) error {
	return call(func() C.NreErrorCode { return C.nreDrawInstance(instance.c()) })
}

// ---------------------------------------------------------------------------
// Scenes
// ---------------------------------------------------------------------------

func CreateScene() (Scene, error) {
	var h C.NreScene
	err := call(func() C.NreErrorCode { return C.nreCreateScene(&h) })
	return sceneFromC(h), err
}

func DestroyScene(scene Scene) error {
	return call(func() C.NreErrorCode { return C.nreDestroyScene(scene.c()) })
}

func BindScene(scene Scene) error {
	return call(func() C.NreErrorCode { return C.nreBindScene(scene.c()) })
}

func GetBoundScene() (Scene, error) {
	var h C.NreScene
	err := call(func() C.NreErrorCode { return C.nreGetBoundScene(&h) })
	return sceneFromC(h), err
}

// AddInstanceToScene adds the instance to the bound scene. Adding one that's already in it does nothing.
func AddInstanceToScene(instance Instance) error {
	return call(func() C.NreErrorCode { return C.nreAddInstanceToScene(instance.c()) })
}

// RemoveInstanceFromScene removes the instance from the bound scene.
func RemoveInstanceFromScene(instance Instance) error {
	return call(func() C.NreErrorCode { return C.nreRemoveInstanceFromScene(instance.c()) })
}

// ---------------------------------------------------------------------------
// Textures and materials. Empty texture handles (Texture{}) use the default texture.
// ---------------------------------------------------------------------------

// CreateTexture loads an image file (PNG, JPEG, ...) as a colour (sRGB) texture, for base colours.
// CreateTextureFull takes srgb.
func CreateTexture(path string) (Texture, error) { return CreateTextureFull(path, true) }

// CreateTextureFull loads an image file. srgb: true for colour images (base colour), false for data maps
// (normal and metallic-roughness maps).
func CreateTextureFull(path string, srgb bool) (Texture, error) {
	p := cString(path)
	defer free(p)
	var h C.NreTexture
	err := call(func() C.NreErrorCode { return C.nreCreateTexture(p, C.bool(srgb), &h) })
	return textureFromC(h), err
}

// DestroyTexture destroys the texture. Give the materials that use it another one first.
func DestroyTexture(texture Texture) error {
	return call(func() C.NreErrorCode { return C.nreDestroyTexture(texture.c()) })
}

// CreateMaterial makes a material with this base texture (Texture{}: the default texture) and no
// metallic or normal texture. CreateMaterialFull takes all three.
func CreateMaterial(baseTexture Texture) (Material, error) {
	return CreateMaterialFull(baseTexture, Texture{}, Texture{})
}

// CreateMaterialFull makes a material with textures as glTF uses them: baseTexture is the colour (sRGB),
// metallicTexture holds roughness in green and metallic in blue, normalTexture is a tangent-space normal map
// (+Y up); load those two with CreateTextureFull(path, false). Texture{} for any of them: white base, flat
// normals, not metal with roughness 1.
func CreateMaterialFull(baseTexture, metallicTexture, normalTexture Texture) (Material, error) {
	var h C.NreMaterial
	err := call(func() C.NreErrorCode {
		return C.nreCreateMaterial(baseTexture.c(), metallicTexture.c(), normalTexture.c(), &h)
	})
	return materialFromC(h), err
}

// DestroyMaterial destroys the material. Models still using it fall back to the default material.
func DestroyMaterial(material Material) error {
	return call(func() C.NreErrorCode { return C.nreDestroyMaterial(material.c()) })
}

// SetMaterialBaseTexture waits for the GPU to finish the frames in flight, so don't call it every frame.
func SetMaterialBaseTexture(material Material, texture Texture) error {
	return call(func() C.NreErrorCode { return C.nreSetMaterialBaseTexture(material.c(), texture.c()) })
}

// GetModelMaterials returns the model's material slots, in the file's material order. Empty for a model
// without materials (like OBJ).
func GetModelMaterials(model Model) ([]Material, error) {
	var count C.uint32_t
	if err := call(func() C.NreErrorCode { return C.nreGetModelMaterials(model.c(), &count, nil) }); err != nil {
		return nil, err
	}
	if count == 0 {
		return []Material{}, nil
	}
	buf := make([]C.NreMaterial, count)
	if err := call(func() C.NreErrorCode { return C.nreGetModelMaterials(model.c(), &count, &buf[0]) }); err != nil {
		return nil, err
	}
	materials := make([]Material, count)
	for i := range materials {
		materials[i] = materialFromC(buf[i])
	}
	return materials, nil
}

// SetModelMaterial makes every mesh of the model use material, replacing all of its slots.
func SetModelMaterial(model Model, material Material) error {
	return call(func() C.NreErrorCode { return C.nreSetModelMaterial(model.c(), material.c()) })
}

// ReplaceModelMaterial replaces slot index (an index into GetModelMaterials).
func ReplaceModelMaterial(model Model, index uint32, material Material) error {
	return call(func() C.NreErrorCode {
		return C.nreReplaceModelMaterial(model.c(), C.uint32_t(index), material.c())
	})
}

// ---------------------------------------------------------------------------
// Lights and the sun. renderer.h's defaults: color {1,1,1}, intensity 3 (directional / sun) or 10 (point).
// direction is the way the light travels (e.g. {0, -1, 0} shines straight down).
// ---------------------------------------------------------------------------

// CreateDirectionalLight makes a white directional light of intensity 3. Add it with AddLightToScene.
func CreateDirectionalLight(direction Vec3) (Light, error) {
	return CreateDirectionalLightFull(direction, Vec3{1, 1, 1}, 3)
}

func CreateDirectionalLightFull(direction, color Vec3, intensity float32) (Light, error) {
	var h C.NreLight
	err := call(func() C.NreErrorCode {
		return C.nreCreateDirectionalLight(direction.c(), color.c(), C.float(intensity), &h)
	})
	return lightFromC(h), err
}

// CreatePointLight makes a white point light of intensity 10 (falls off with 1/distance²).
func CreatePointLight(position Vec3) (Light, error) {
	return CreatePointLightFull(position, Vec3{1, 1, 1}, 10)
}

func CreatePointLightFull(position, color Vec3, intensity float32) (Light, error) {
	var h C.NreLight
	err := call(func() C.NreErrorCode {
		return C.nreCreatePointLight(position.c(), color.c(), C.float(intensity), &h)
	})
	return lightFromC(h), err
}

// DestroyLight destroys the light and removes it from every scene it's in.
func DestroyLight(light Light) error {
	return call(func() C.NreErrorCode { return C.nreDestroyLight(light.c()) })
}

// AddLightToScene adds the light to the bound scene. Adding one that's already in it does nothing.
func AddLightToScene(light Light) error {
	return call(func() C.NreErrorCode { return C.nreAddLightToScene(light.c()) })
}

func RemoveLightFromScene(light Light) error {
	return call(func() C.NreErrorCode { return C.nreRemoveLightFromScene(light.c()) })
}

// SetLightPosition is only used by point lights.
func SetLightPosition(light Light, position Vec3) error {
	return call(func() C.NreErrorCode { return C.nreSetLightPosition(light.c(), position.c()) })
}

// SetLightDirection is only used by directional lights.
func SetLightDirection(light Light, direction Vec3) error {
	return call(func() C.NreErrorCode { return C.nreSetLightDirection(light.c(), direction.c()) })
}

func SetLightColor(light Light, color Vec3, intensity float32) error {
	return call(func() C.NreErrorCode { return C.nreSetLightColor(light.c(), color.c(), C.float(intensity)) })
}

// SetSun sets the bound scene's sun to a white light of intensity 3 travelling in direction. A scene
// starts without one. SetSunFull takes the color and intensity.
func SetSun(direction Vec3) error { return SetSunFull(direction, Vec3{1, 1, 1}, 3) }

func SetSunFull(direction, color Vec3, intensity float32) error {
	return call(func() C.NreErrorCode { return C.nreSetSun(direction.c(), color.c(), C.float(intensity)) })
}

// SetSunDirection keeps the sun's color and intensity, e.g. for a day/night cycle.
func SetSunDirection(direction Vec3) error {
	return call(func() C.NreErrorCode { return C.nreSetSunDirection(direction.c()) })
}

func GetSunDirection() (Vec3, error) {
	var v C.NreVec3
	err := call(func() C.NreErrorCode { return C.nreGetSunDirection(&v) })
	return vec3FromC(v), err
}

// ---------------------------------------------------------------------------
// Scene lighting, shadows, haze, exposure and sky colour, on the bound scene. renderer.h's defaults:
// Small: shadows 20 m, haze off. Landscape: shadows 500 m, haze 8000 m / 300 m. Exposure 1, sky {0.1, 0.3, 0.6}.
// ---------------------------------------------------------------------------

// SetSceneLighting switches the bound scene's lighting mode, and resets shadow distance, haze and the sky
// to the mode's defaults (the sun, lights, exposure and sky colour are kept).
func SetSceneLighting(lighting SceneLighting) error {
	return call(func() C.NreErrorCode { return C.nreSetSceneLighting(C.NreSceneLighting(lighting)) })
}

func GetSceneLighting() (SceneLighting, error) {
	var v C.NreSceneLighting
	err := call(func() C.NreErrorCode { return C.nreGetSceneLighting(&v) })
	return SceneLighting(v), err
}

// SetShadowDistance sets how far from the camera the sun casts shadows, in metres. 0 turns sun shadows
// off; negative is ErrInvalidArgument.
func SetShadowDistance(meters float32) error {
	return call(func() C.NreErrorCode { return C.nreSetShadowDistance(C.float(meters)) })
}

// SetHaze sets the distance haze with a 300 m thick layer. visibility: metres at which the haze hides almost
// everything, 0 = off. SetHazeFull takes the height.
func SetHaze(visibility float32) error { return SetHazeFull(visibility, 300) }

// SetHazeFull is SetHaze with the haze layer's thickness (height, greater than 0): it thins out above that
// many metres above y = 0.
func SetHazeFull(visibility, height float32) error {
	return call(func() C.NreErrorCode { return C.nreSetHaze(C.float(visibility), C.float(height)) })
}

// SetExposure sets the bound scene's exposure: greater than 0, 2 is twice as bright. Default 1.
func SetExposure(exposure float32) error {
	return call(func() C.NreErrorCode { return C.nreSetExposure(C.float(exposure)) })
}

// SetSkyColor sets the background without a skybox (Small), linear 0..1.
func SetSkyColor(color Vec3) error {
	return call(func() C.NreErrorCode { return C.nreSetSkyColor(color.c()) })
}

// ---------------------------------------------------------------------------
// Cubemaps and the skybox
// ---------------------------------------------------------------------------

// CreateCubemap loads six square images of the same size as color (sRGB), in the OpenGL order:
// right (+X), left (-X), top (+Y), bottom (-Y), front (+Z), back (-Z).
func CreateCubemap(right, left, top, bottom, front, back string) (Cubemap, error) {
	r, l, t, b, f, k := cString(right), cString(left), cString(top), cString(bottom), cString(front), cString(back)
	defer func() {
		free(r)
		free(l)
		free(t)
		free(b)
		free(f)
		free(k)
	}()
	var h C.NreCubemap
	err := call(func() C.NreErrorCode { return C.nreCreateCubemap(r, l, t, b, f, k, &h) })
	return cubemapFromC(h), err
}

// DestroyCubemap waits for the GPU to finish the frames in flight. Scenes using it go back to no skybox.
func DestroyCubemap(cubemap Cubemap) error {
	return call(func() C.NreErrorCode { return C.nreDestroyCubemap(cubemap.c()) })
}

// SetSkybox sets the bound scene's sky (3D only). Cubemap{} removes it.
func SetSkybox(cubemap Cubemap) error {
	return call(func() C.NreErrorCode { return C.nreSetSkybox(cubemap.c()) })
}

// ---------------------------------------------------------------------------
// Camera. Angles are in radians.
// ---------------------------------------------------------------------------

func SetCameraPosition(position Vec3) error {
	return call(func() C.NreErrorCode { return C.nreSetCameraPosition(position.c()) })
}

func GetCameraPosition() (Vec3, error) {
	var v C.NreVec3
	err := call(func() C.NreErrorCode { return C.nreGetCameraPosition(&v) })
	return vec3FromC(v), err
}

func MoveCamera(offset Vec3) error {
	return call(func() C.NreErrorCode { return C.nreMoveCamera(offset.c()) })
}

func MoveCameraRelative(offset Vec3) error {
	return call(func() C.NreErrorCode { return C.nreMoveCameraRelative(offset.c()) })
}

func SetCameraRotation(yaw, pitch float32) error {
	return call(func() C.NreErrorCode { return C.nreSetCameraRotation(C.float(yaw), C.float(pitch)) })
}

func RotateCamera(yawDelta, pitchDelta float32) error {
	return call(func() C.NreErrorCode { return C.nreRotateCamera(C.float(yawDelta), C.float(pitchDelta)) })
}

func CameraLookAt(target Vec3) error {
	return call(func() C.NreErrorCode { return C.nreCameraLookAt(target.c()) })
}

func GetCameraForward() (Vec3, error) {
	var v C.NreVec3
	err := call(func() C.NreErrorCode { return C.nreGetCameraForward(&v) })
	return vec3FromC(v), err
}

func GetViewMatrix() (Mat4, error) {
	var m C.NreMat4
	err := call(func() C.NreErrorCode { return C.nreGetViewMatrix(&m) })
	return mat4FromC(m), err
}

// GetProjectionMatrix returns the projection the scene is drawn with. Vulkan conventions: clip-space +Y down,
// reverse-Z with an infinite far plane. See renderer.h.
func GetProjectionMatrix() (Mat4, error) {
	var m C.NreMat4
	err := call(func() C.NreErrorCode { return C.nreGetProjectionMatrix(&m) })
	return mat4FromC(m), err
}

// PickInstance returns the nearest visible instance of the bound scene under a pixel (GetMousePosition
// units), with ok false when nothing is there. Render3D only.
func PickInstance(windowPos Vec2) (hit RayHit, ok bool, err error) {
	var h C.NreRayHit
	var b C.bool
	err = call(func() C.NreErrorCode { return C.nrePickInstance(windowPos.c(), &b, &h) })
	if err != nil || !bool(b) {
		return RayHit{}, false, err
	}
	return rayHitFromC(h), true, nil
}

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------

// GetFPS returns the frames per second, averaged over the last half second.
func GetFPS() (float32, error) {
	var v C.float
	err := call(func() C.NreErrorCode { return C.nreGetFPS(&v) })
	return float32(v), err
}

// GetDeltaTime returns the seconds between the last two presented frames.
func GetDeltaTime() (float32, error) {
	var v C.float
	err := call(func() C.NreErrorCode { return C.nreGetDeltaTime(&v) })
	return float32(v), err
}

// ---------------------------------------------------------------------------
// Input. The deltas and pressed/released cover the last PollEvents call. See renderer.h.
// ---------------------------------------------------------------------------

func IsMouseButtonDown(button MouseButton) (bool, error) {
	var v C.bool
	err := call(func() C.NreErrorCode { return C.nreIsMouseButtonDown(C.NreMouseButton(button), &v) })
	return bool(v), err
}

// IsMouseButtonPressed: the button went down during the last PollEvents call (true for one frame).
func IsMouseButtonPressed(button MouseButton) (bool, error) {
	var v C.bool
	err := call(func() C.NreErrorCode { return C.nreIsMouseButtonPressed(C.NreMouseButton(button), &v) })
	return bool(v), err
}

// IsMouseButtonReleased: the button went up during the last PollEvents call (true for one frame).
func IsMouseButtonReleased(button MouseButton) (bool, error) {
	var v C.bool
	err := call(func() C.NreErrorCode { return C.nreIsMouseButtonReleased(C.NreMouseButton(button), &v) })
	return bool(v), err
}

// GetMouseDelta returns how far the cursor moved, in pixels. +X is right, +Y is down.
func GetMouseDelta() (Vec2, error) {
	var v C.NreVec2
	err := call(func() C.NreErrorCode { return C.nreGetMouseDelta(&v) })
	return vec2FromC(v), err
}

// GetScrollDelta returns scroll wheel steps. Positive means scrolling up / away from you.
func GetScrollDelta() (float32, error) {
	var v C.float
	err := call(func() C.NreErrorCode { return C.nreGetScrollDelta(&v) })
	return float32(v), err
}

// GetMousePosition returns the cursor position in pixels from the window's top-left corner.
func GetMousePosition() (Vec2, error) {
	var v C.NreVec2
	err := call(func() C.NreErrorCode { return C.nreGetMousePosition(&v) })
	return vec2FromC(v), err
}

func SetCursorMode(mode CursorMode) error {
	return call(func() C.NreErrorCode { return C.nreSetCursorMode(C.NreCursorMode(mode)) })
}

func IsKeyDown(key Key) (bool, error) {
	var v C.bool
	err := call(func() C.NreErrorCode { return C.nreIsKeyDown(C.NreKey(key), &v) })
	return bool(v), err
}

// IsKeyPressed: the key went down during the last PollEvents call. Holding a key doesn't repeat it.
func IsKeyPressed(key Key) (bool, error) {
	var v C.bool
	err := call(func() C.NreErrorCode { return C.nreIsKeyPressed(C.NreKey(key), &v) })
	return bool(v), err
}

// IsKeyReleased: the key went up during the last PollEvents call.
func IsKeyReleased(key Key) (bool, error) {
	var v C.bool
	err := call(func() C.NreErrorCode { return C.nreIsKeyReleased(C.NreKey(key), &v) })
	return bool(v), err
}
