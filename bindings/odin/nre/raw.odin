package nre

// The raw C API: every function of include/nre.h, with its C name minus the "nre" prefix
// (nreCreateModel -> nre.CreateModel). Prefer the snake_case wrappers in nre.odin; these are here for
// code that wants the exact C signatures. See nre.h for the contract and renderer.h for what each one does.

import "core:c"
import "core:fmt"
import "core:os"
import "base:runtime"

// How to find libnre_c at link time. Odin can't read environment variables in a foreign import, so the
// default is the system library "nre_c" (-lnre_c) and the directory comes from the linker flags:
//
//     odin build . -extra-linker-flags:"-L$NRE_LIB_DIR -Wl,-rpath,$NRE_LIB_DIR"
//
// (bindings/odin/run.sh does this). The program then loads the library by its SONAME (libnre_c.so.0.3 on
// Linux) through that rpath, or the system's normal search when the rpath is left out.
// -define:NRE_C_LIB="system:<name>" links another library name (e.g. on Windows); a file path there is rejected
// by the Odin compiler ("Invalid import path"), so use -L / -rpath for the directory.
NRE_C_LIB :: #config(NRE_C_LIB, "system:nre_c.lib" when ODIN_OS == .Windows else "system:nre_c")

foreign import nre_lib { NRE_C_LIB }

// The ABI these bindings were written against (nre.h of NRE 0.3). Checked at startup.
ABI_VERSION_MAJOR :: 0
ABI_VERSION_MINOR :: 3

@(default_calling_convention = "c", link_prefix = "nre")
foreign nre_lib {
	// Errors and version
	GetLastErrorMessage :: proc() -> cstring ---
	ErrorCodeToString   :: proc(code: Error_Code) -> cstring ---
	GetVersion          :: proc(major, minor, patch: ^u32) --- // any may be nil

	// Setup and frames
	SetRenderConfig :: proc(mode: Render_Mode, apiMode: Api_Mode, errorPolicy: Render_Error_Policy,
	                        manualFramePresentationAndSubmission: bool,
	                        windowWidth, windowHeight: c.int, windowTitle: cstring,
	                        resizable: bool, showFpsInTitle: bool) -> Error_Code ---
	Init              :: proc() -> Error_Code ---
	Shutdown          :: proc() -> Error_Code ---
	RenderFrame       :: proc() -> Error_Code ---
	EndFrame          :: proc() -> Error_Code ---
	PresentFrame      :: proc() -> Error_Code ---
	SubmitFrame       :: proc() -> Error_Code ---
	WindowShouldClose :: proc(shouldClose: ^bool) -> Error_Code ---
	PollEvents        :: proc() -> Error_Code ---

	// Shaders and pipelines
	CreateShader    :: proc(shaderPath: cstring, type: Shader_Type, shader: ^Shader) -> Error_Code ---
	DestroyShader   :: proc(shader: Shader) -> Error_Code ---
	CreatePipeline  :: proc(vertexShader, fragmentShader: Shader, pipeline: ^Pipeline) -> Error_Code ---
	DestroyPipeline :: proc(pipeline: Pipeline) -> Error_Code ---
	BindPipeline    :: proc(pipeline: Pipeline) -> Error_Code ---

	// Models and instances
	CreateModel          :: proc(modelPath: cstring, model: ^Model) -> Error_Code ---
	// The arrays are copied; material {} = the default one.
	CreateMeshModel      :: proc(vertices: [^]Vertex, vertexCount: u32, indices: [^]u32, indexCount: u32,
	                             material: Material, model: ^Model) -> Error_Code ---
	DestroyModel         :: proc(model: Model) -> Error_Code ---
	CreateInstance       :: proc(model: Model, instance: ^Instance) -> Error_Code ---
	DestroyInstance      :: proc(instance: Instance) -> Error_Code ---
	SetInstanceTransform :: proc(instance: Instance, transform: ^Transform) -> Error_Code ---
	GetInstanceTransform :: proc(instance: Instance, transform: ^Transform) -> Error_Code ---
	GetInstanceMatrix    :: proc(instance: Instance, matrix_: ^Mat4) -> Error_Code ---
	TranslateInstance    :: proc(instance: Instance, offset: Vec3) -> Error_Code ---
	RotateInstance       :: proc(instance: Instance, angle: f32, axis: Vec3) -> Error_Code ---
	ScaleInstance        :: proc(instance: Instance, factor: Vec3) -> Error_Code ---
	SetVisibility        :: proc(instance: Instance, visible: bool) -> Error_Code ---
	SetCastsShadows      :: proc(instance: Instance, castsShadows: bool) -> Error_Code ---
	DrawModel            :: proc(model: Model, transform: ^Transform) -> Error_Code --- // transform may be nil: identity
	DrawInstance         :: proc(instance: Instance) -> Error_Code ---

	// Scenes
	CreateScene             :: proc(scene: ^Scene) -> Error_Code ---
	DestroyScene            :: proc(scene: Scene) -> Error_Code ---
	BindScene               :: proc(scene: Scene) -> Error_Code ---
	GetBoundScene           :: proc(scene: ^Scene) -> Error_Code ---
	AddInstanceToScene      :: proc(instance: Instance) -> Error_Code ---
	RemoveInstanceFromScene :: proc(instance: Instance) -> Error_Code ---

	// Textures and materials
	CreateTexture          :: proc(path: cstring, srgb: bool, texture: ^Texture) -> Error_Code ---
	DestroyTexture         :: proc(texture: Texture) -> Error_Code ---
	CreateMaterial         :: proc(baseTexture, metallicTexture, normalTexture: Texture, material: ^Material) -> Error_Code ---
	DestroyMaterial        :: proc(material: Material) -> Error_Code ---
	SetMaterialBaseTexture :: proc(material: Material, texture: Texture) -> Error_Code ---
	// With materials nil, sets count to the number of slots. Otherwise count is the array's size; it writes up
	// to that many and sets count to how many it wrote.
	GetModelMaterials      :: proc(model: Model, count: ^u32, materials: [^]Material) -> Error_Code ---
	SetModelMaterial       :: proc(model: Model, material: Material) -> Error_Code ---
	ReplaceModelMaterial   :: proc(model: Model, index: u32, material: Material) -> Error_Code ---

	// Lights and the sun
	CreateDirectionalLight :: proc(direction, color: Vec3, intensity: f32, light: ^Light) -> Error_Code ---
	CreatePointLight       :: proc(position, color: Vec3, intensity: f32, light: ^Light) -> Error_Code ---
	DestroyLight           :: proc(light: Light) -> Error_Code ---
	AddLightToScene        :: proc(light: Light) -> Error_Code ---
	RemoveLightFromScene   :: proc(light: Light) -> Error_Code ---
	SetLightPosition       :: proc(light: Light, position: Vec3) -> Error_Code ---
	SetLightDirection      :: proc(light: Light, direction: Vec3) -> Error_Code ---
	SetLightColor          :: proc(light: Light, color: Vec3, intensity: f32) -> Error_Code ---
	SetSun                 :: proc(direction, color: Vec3, intensity: f32) -> Error_Code ---
	SetSunDirection        :: proc(direction: Vec3) -> Error_Code ---
	GetSunDirection        :: proc(direction: ^Vec3) -> Error_Code ---

	// Scene lighting, shadows, haze, exposure and sky colour (on the bound scene)
	SetSceneLighting  :: proc(lighting: Scene_Lighting) -> Error_Code ---
	// May write a value outside Scene_Lighting (a newer library): see get_scene_lighting.
	GetSceneLighting  :: proc(lighting: ^Scene_Lighting) -> Error_Code ---
	SetShadowDistance :: proc(meters: f32) -> Error_Code ---
	SetHaze           :: proc(visibility, height: f32) -> Error_Code ---
	SetExposure       :: proc(exposure: f32) -> Error_Code ---
	SetSkyColor       :: proc(color: Vec3) -> Error_Code ---

	// Cubemaps and the skybox
	CreateCubemap  :: proc(right, left, top, bottom, front, back: cstring, cubemap: ^Cubemap) -> Error_Code ---
	DestroyCubemap :: proc(cubemap: Cubemap) -> Error_Code ---
	SetSkybox      :: proc(cubemap: Cubemap) -> Error_Code --- // empty handle: no skybox

	// Camera
	SetCameraPosition   :: proc(position: Vec3) -> Error_Code ---
	GetCameraPosition   :: proc(position: ^Vec3) -> Error_Code ---
	MoveCamera          :: proc(offset: Vec3) -> Error_Code ---
	MoveCameraRelative  :: proc(offset: Vec3) -> Error_Code ---
	SetCameraRotation   :: proc(yaw, pitch: f32) -> Error_Code ---
	RotateCamera        :: proc(yawDelta, pitchDelta: f32) -> Error_Code ---
	CameraLookAt        :: proc(target: Vec3) -> Error_Code ---
	GetCameraForward    :: proc(forward: ^Vec3) -> Error_Code ---
	GetViewMatrix       :: proc(view: ^Mat4) -> Error_Code ---
	GetProjectionMatrix :: proc(projection: ^Mat4) -> Error_Code ---
	// Sets hit; when it's true, rayHit holds the hit, otherwise rayHit is zeroed.
	PickInstance        :: proc(windowPos: Vec2, hit: ^bool, rayHit: ^Ray_Hit) -> Error_Code ---

	// Timing
	GetFPS       :: proc(fps: ^f32) -> Error_Code ---
	GetDeltaTime :: proc(deltaTime: ^f32) -> Error_Code ---

	// Input
	IsMouseButtonDown     :: proc(button: Mouse_Button, down: ^bool) -> Error_Code ---
	IsMouseButtonPressed  :: proc(button: Mouse_Button, pressed: ^bool) -> Error_Code ---
	IsMouseButtonReleased :: proc(button: Mouse_Button, released: ^bool) -> Error_Code ---
	GetMouseDelta         :: proc(delta: ^Vec2) -> Error_Code ---
	GetScrollDelta        :: proc(delta: ^f32) -> Error_Code ---
	GetMousePosition      :: proc(position: ^Vec2) -> Error_Code ---
	SetCursorMode         :: proc(mode: Cursor_Mode) -> Error_Code ---
	IsKeyDown             :: proc(key: Key, down: ^bool) -> Error_Code ---
	IsKeyPressed          :: proc(key: Key, pressed: ^bool) -> Error_Code ---
	IsKeyReleased         :: proc(key: Key, released: ^bool) -> Error_Code ---
}

// Runs before main: stops the program if the loaded libnre_c isn't the ABI version these bindings target.
@(init, private)
check_abi_version :: proc "contextless" () {
	major, minor, patch: u32
	GetVersion(&major, &minor, &patch)
	if major != ABI_VERSION_MAJOR || minor != ABI_VERSION_MINOR {
		context = runtime.default_context()
		fmt.eprintfln("nre: libnre_c is version %v.%v.%v, but these Odin bindings target %v.%v (nre.h). " +
		              "Point NRE_LIB_DIR at a matching build.", major, minor, patch, ABI_VERSION_MAJOR, ABI_VERSION_MINOR)
		os.exit(1)
	}
}
