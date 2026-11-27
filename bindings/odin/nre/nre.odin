/*
Odin bindings for NRE, a Vulkan renderer, over its C API (include/nre.h, libnre_c).

Same functions as renderer.h in Odin style (NRE::createModel -> nre.create_model); see renderer.h for what
each one does. Every wrapper returns an Error_Code as its last result (.Ok on success) and values before it,
so `or_return` works:

	run :: proc() -> (err: nre.Error_Code) {
		model := nre.create_model("box.glb") or_return
		...
	}

On failure nre.last_error_message() describes it. The window comes from GLFW, so call set_render_config,
init, poll_events, render_frame and the input functions from the main thread.

Strings are converted to C strings with context.allocator for the duration of the call only.
*/
package nre

import "core:c"
import "core:strings"

// Turns a code from the C library into a known Error_Code: values outside the enum become .Unknown.
normalize_error_code :: #force_inline proc "contextless" (code: Error_Code) -> Error_Code {
	return code if code >= min(Error_Code) && code <= max(Error_Code) else .Unknown
}

// Turns a lighting mode from the C library into a known Scene_Lighting: values outside the enum (from a
// newer library) become .Small, the default mode.
normalize_scene_lighting :: #force_inline proc "contextless" (lighting: Scene_Lighting) -> Scene_Lighting {
	return lighting if lighting >= min(Scene_Lighting) && lighting <= max(Scene_Lighting) else .Small
}

@(private)
check :: #force_inline proc "contextless" (code: Error_Code) -> Error_Code {
	return normalize_error_code(code)
}

// The message of the most recent failed call on this thread ("" if none failed yet). It points into the
// library and stays valid until the next failed call on this thread: strings.clone it to keep it.
last_error_message :: proc "contextless" () -> string {
	return string(GetLastErrorMessage())
}

// "Ok", "FileNotFound", ... A static string from the library.
error_code_to_string :: proc "contextless" (code: Error_Code) -> string {
	return string(ErrorCodeToString(code))
}

// The loaded library's version. The bindings already checked at startup that major.minor is
// ABI_VERSION_MAJOR.ABI_VERSION_MINOR.
get_version :: proc "contextless" () -> (major, minor, patch: u32) {
	GetVersion(&major, &minor, &patch)
	return
}

// ---------------------------------------------------------------------------
// Setup and frames
// ---------------------------------------------------------------------------

// Call before init(). Main thread only. See renderer.h's RenderConfig / WindowConfig.
set_render_config :: proc(
	width, height: int,
	title: string,
	mode := Render_Mode.Render_3D,
	api_mode := Api_Mode.Scene,
	error_policy := Render_Error_Policy.Return,
	manual_frame_presentation_and_submission := false,
	resizable := true,
	show_fps_in_title := true,
) -> Error_Code {
	ctitle := strings.clone_to_cstring(title)
	defer delete(ctitle)
	return check(SetRenderConfig(mode, api_mode, error_policy, manual_frame_presentation_and_submission,
	                             c.int(width), c.int(height), ctitle, resizable, show_fps_in_title))
}

// Opens the window and sets up Vulkan. Main thread only.
init :: proc "contextless" () -> Error_Code { return check(Init()) }
shutdown :: proc "contextless" () -> Error_Code { return check(Shutdown()) }
// Scene mode: draws the bound scene (and submits/presents it unless manual presentation was asked for).
// Function_Draw mode: starts the frame; then draw_model()/draw_instance(), then end_frame(). See renderer.h.
render_frame :: proc "contextless" () -> Error_Code { return check(RenderFrame()) }
// Function_Draw mode only: ends the frame render_frame() started.
end_frame :: proc "contextless" () -> Error_Code { return check(EndFrame()) }
present_frame :: proc "contextless" () -> Error_Code { return check(PresentFrame()) }
submit_frame :: proc "contextless" () -> Error_Code { return check(SubmitFrame()) }

window_should_close :: proc "contextless" () -> (should_close: bool, err: Error_Code) {
	err = check(WindowShouldClose(&should_close))
	return
}

// Main thread only. The mouse and key "pressed/released/delta" values cover the last call.
poll_events :: proc "contextless" () -> Error_Code { return check(PollEvents()) }

// ---------------------------------------------------------------------------
// Shaders and pipelines
// ---------------------------------------------------------------------------

// Loads a compiled SPIR-V file.
create_shader :: proc(path: string, type: Shader_Type) -> (shader: Shader, err: Error_Code) {
	cpath := strings.clone_to_cstring(path)
	defer delete(cpath)
	err = check(CreateShader(cpath, type, &shader))
	return
}

destroy_shader :: proc "contextless" (shader: Shader) -> Error_Code { return check(DestroyShader(shader)) }

create_pipeline :: proc "contextless" (vertex_shader, fragment_shader: Shader) -> (pipeline: Pipeline, err: Error_Code) {
	err = check(CreatePipeline(vertex_shader, fragment_shader, &pipeline))
	return
}

destroy_pipeline :: proc "contextless" (pipeline: Pipeline) -> Error_Code { return check(DestroyPipeline(pipeline)) }
bind_pipeline :: proc "contextless" (pipeline: Pipeline) -> Error_Code { return check(BindPipeline(pipeline)) }

// ---------------------------------------------------------------------------
// Models and instances
// ---------------------------------------------------------------------------

// A Model is the loaded file and isn't drawn by itself: draw it through instances (see renderer.h).
create_model :: proc(path: string) -> (model: Model, err: Error_Code) {
	cpath := strings.clone_to_cstring(path)
	defer delete(cpath)
	err = check(CreateModel(cpath, &model))
	return
}

// A model from your own geometry (terrain, water, editor meshes): a triangle list, counter-clockwise front
// faces, indices into vertices. Vertices whose normal is {0, 0, 0} get one computed from the triangles that
// use them. The empty material {} uses the default one. The library copies the arrays (no copy is made here).
create_mesh_model :: proc "contextless" (vertices: []Vertex, indices: []u32, material := Material{}) -> (model: Model, err: Error_Code) {
	err = check(CreateMeshModel(raw_data(vertices), u32(len(vertices)), raw_data(indices), u32(len(indices)), material, &model))
	return
}

// Instances of a destroyed model stay valid but draw nothing.
destroy_model :: proc "contextless" (model: Model) -> Error_Code { return check(DestroyModel(model)) }

create_instance :: proc "contextless" (model: Model) -> (instance: Instance, err: Error_Code) {
	err = check(CreateInstance(model, &instance))
	return
}

// Also removes it from every scene it's in.
destroy_instance :: proc "contextless" (instance: Instance) -> Error_Code { return check(DestroyInstance(instance)) }

set_instance_transform :: proc "contextless" (instance: Instance, transform: Transform) -> Error_Code {
	t := transform
	return check(SetInstanceTransform(instance, &t))
}

get_instance_transform :: proc "contextless" (instance: Instance) -> (transform: Transform, err: Error_Code) {
	err = check(GetInstanceTransform(instance, &transform))
	return
}

get_instance_matrix :: proc "contextless" (instance: Instance) -> (m: Mat4, err: Error_Code) {
	err = check(GetInstanceMatrix(instance, &m))
	return
}

translate_instance :: proc "contextless" (instance: Instance, offset: Vec3) -> Error_Code {
	return check(TranslateInstance(instance, offset))
}

// angle is in radians.
rotate_instance :: proc "contextless" (instance: Instance, angle: f32, axis: Vec3) -> Error_Code {
	return check(RotateInstance(instance, angle, axis))
}

scale_instance :: proc "contextless" (instance: Instance, factor: Vec3) -> Error_Code {
	return check(ScaleInstance(instance, factor))
}

// A hidden instance stays in its scenes but isn't drawn.
set_visibility :: proc "contextless" (instance: Instance, visible: bool) -> Error_Code {
	return check(SetVisibility(instance, visible))
}

// Whether the instance casts sun shadows (it still receives them). Default true; e.g. off for grass.
set_casts_shadows :: proc "contextless" (instance: Instance, casts_shadows: bool) -> Error_Code {
	return check(SetCastsShadows(instance, casts_shadows))
}

// Function_Draw mode only, between render_frame() and end_frame(): draws the model once, no instance needed.
draw_model :: proc "contextless" (model: Model, transform := TRANSFORM_IDENTITY) -> Error_Code {
	t := transform
	return check(DrawModel(model, &t))
}

// Function_Draw mode only. Uses the instance's transform; a hidden instance isn't drawn.
draw_instance :: proc "contextless" (instance: Instance) -> Error_Code { return check(DrawInstance(instance)) }

// ---------------------------------------------------------------------------
// Scenes
// ---------------------------------------------------------------------------

create_scene :: proc "contextless" () -> (scene: Scene, err: Error_Code) {
	err = check(CreateScene(&scene))
	return
}

destroy_scene :: proc "contextless" (scene: Scene) -> Error_Code { return check(DestroyScene(scene)) }
bind_scene :: proc "contextless" (scene: Scene) -> Error_Code { return check(BindScene(scene)) }

get_bound_scene :: proc "contextless" () -> (scene: Scene, err: Error_Code) {
	err = check(GetBoundScene(&scene))
	return
}

// Adds to / removes from the bound scene. Adding one that's already in it does nothing.
add_instance_to_scene :: proc "contextless" (instance: Instance) -> Error_Code {
	return check(AddInstanceToScene(instance))
}

remove_instance_from_scene :: proc "contextless" (instance: Instance) -> Error_Code {
	return check(RemoveInstanceFromScene(instance))
}

// ---------------------------------------------------------------------------
// Textures and materials. Empty texture handles ({}) use the default texture.
// Materials belong to the model, so changing one changes every instance of it (see renderer.h).
// ---------------------------------------------------------------------------

// Loads an image file (PNG, JPEG, ...). srgb = true for colour images (base colour); false for data maps
// (normal and metallic-roughness maps).
create_texture :: proc(path: string, srgb := true) -> (texture: Texture, err: Error_Code) {
	cpath := strings.clone_to_cstring(path)
	defer delete(cpath)
	err = check(CreateTexture(cpath, srgb, &texture))
	return
}

// Give the materials that use this texture another one first: they keep pointing at it.
destroy_texture :: proc "contextless" (texture: Texture) -> Error_Code { return check(DestroyTexture(texture)) }

// Textures as glTF uses them: base_texture is the colour (sRGB), metallic_texture holds roughness in green and
// metallic in blue, normal_texture is a tangent-space normal map; load those two with srgb = false.
create_material :: proc "contextless" (
	base_texture := Texture{},
	metallic_texture := Texture{},
	normal_texture := Texture{},
) -> (material: Material, err: Error_Code) {
	err = check(CreateMaterial(base_texture, metallic_texture, normal_texture, &material))
	return
}

// Models still using it fall back to the default material.
destroy_material :: proc "contextless" (material: Material) -> Error_Code { return check(DestroyMaterial(material)) }

// Waits for the GPU to finish the frames in flight, so don't call it every frame.
set_material_base_texture :: proc "contextless" (material: Material, texture: Texture) -> Error_Code {
	return check(SetMaterialBaseTexture(material, texture))
}

// The model's material slots, in the file's material order (empty for e.g. OBJ). The slice is allocated
// with `allocator`; delete it when done (the materials themselves belong to the model).
get_model_materials :: proc(model: Model, allocator := context.allocator) -> (materials: []Material, err: Error_Code) {
	count: u32
	check(GetModelMaterials(model, &count, nil)) or_return
	if count == 0 do return nil, .Ok
	materials = make([]Material, count, allocator)
	if err = check(GetModelMaterials(model, &count, raw_data(materials))); err != .Ok {
		delete(materials, allocator)
		return nil, err
	}
	return materials[:count], .Ok
}

// Makes every mesh of the model use material, replacing all of its slots.
set_model_material :: proc "contextless" (model: Model, material: Material) -> Error_Code {
	return check(SetModelMaterial(model, material))
}

// Replaces slot index (an index into get_model_materials), so only the meshes using that slot change.
replace_model_material :: proc "contextless" (model: Model, index: u32, material: Material) -> Error_Code {
	return check(ReplaceModelMaterial(model, index, material))
}

// ---------------------------------------------------------------------------
// Lights and the sun. direction is the way the light travels, e.g. {0, -1, 0} shines straight down.
// Create a light, then add it to the bound scene (see renderer.h).
// ---------------------------------------------------------------------------

create_directional_light :: proc "contextless" (direction: Vec3, color := Vec3{1, 1, 1}, intensity: f32 = 3) -> (light: Light, err: Error_Code) {
	err = check(CreateDirectionalLight(direction, color, intensity, &light))
	return
}

// Falls off with 1/distance².
create_point_light :: proc "contextless" (position: Vec3, color := Vec3{1, 1, 1}, intensity: f32 = 10) -> (light: Light, err: Error_Code) {
	err = check(CreatePointLight(position, color, intensity, &light))
	return
}

// Also removes it from every scene it's in.
destroy_light :: proc "contextless" (light: Light) -> Error_Code { return check(DestroyLight(light)) }
add_light_to_scene :: proc "contextless" (light: Light) -> Error_Code { return check(AddLightToScene(light)) }
remove_light_from_scene :: proc "contextless" (light: Light) -> Error_Code { return check(RemoveLightFromScene(light)) }

// Only used by point lights.
set_light_position :: proc "contextless" (light: Light, position: Vec3) -> Error_Code {
	return check(SetLightPosition(light, position))
}

// Only used by directional lights.
set_light_direction :: proc "contextless" (light: Light, direction: Vec3) -> Error_Code {
	return check(SetLightDirection(light, direction))
}

set_light_color :: proc "contextless" (light: Light, color: Vec3, intensity: f32) -> Error_Code {
	return check(SetLightColor(light, color, intensity))
}

// The bound scene's sun: one directional light kept apart from the others (see renderer.h).
set_sun :: proc "contextless" (direction: Vec3, color := Vec3{1, 1, 1}, intensity: f32 = 3) -> Error_Code {
	return check(SetSun(direction, color, intensity))
}

// Keeps the color and intensity, e.g. for a day/night cycle.
set_sun_direction :: proc "contextless" (direction: Vec3) -> Error_Code { return check(SetSunDirection(direction)) }

get_sun_direction :: proc "contextless" () -> (direction: Vec3, err: Error_Code) {
	err = check(GetSunDirection(&direction))
	return
}

// ---------------------------------------------------------------------------
// Scene lighting, shadows, haze, exposure and sky colour, on the bound scene (see renderer.h). Defaults:
// Small: shadows 20 m, haze off. Landscape: shadows 500 m, haze 8000 m / 300 m. Exposure 1, sky {0.1, 0.3, 0.6}.
// ---------------------------------------------------------------------------

// Also resets shadow distance, haze and the sky to the mode's defaults.
set_scene_lighting :: proc "contextless" (lighting: Scene_Lighting) -> Error_Code {
	return check(SetSceneLighting(lighting))
}

get_scene_lighting :: proc "contextless" () -> (lighting: Scene_Lighting, err: Error_Code) {
	err = check(GetSceneLighting(&lighting))
	lighting = normalize_scene_lighting(lighting)
	return
}

// Metres; 0 turns sun shadows off, negative is .Invalid_Argument.
set_shadow_distance :: proc "contextless" (meters: f32) -> Error_Code { return check(SetShadowDistance(meters)) }

// visibility: metres at which the haze hides almost everything, 0 = off. height: the haze layer's thickness,
// greater than 0.
set_haze :: proc "contextless" (visibility: f32, height: f32 = 300) -> Error_Code {
	return check(SetHaze(visibility, height))
}

// Greater than 0; 2 is twice as bright.
set_exposure :: proc "contextless" (exposure: f32) -> Error_Code { return check(SetExposure(exposure)) }

// The background without a skybox (Small), linear 0..1.
set_sky_color :: proc "contextless" (color: Vec3) -> Error_Code { return check(SetSkyColor(color)) }

// ---------------------------------------------------------------------------
// Cubemaps and the skybox
// ---------------------------------------------------------------------------

// Six square images of the same size, in the OpenGL order: right (+X), left (-X), top (+Y), bottom (-Y),
// front (+Z), back (-Z). Loaded as color (sRGB).
create_cubemap :: proc(right, left, top, bottom, front, back: string) -> (cubemap: Cubemap, err: Error_Code) {
	cs: [6]cstring
	for path, i in ([6]string{right, left, top, bottom, front, back}) {
		cs[i] = strings.clone_to_cstring(path)
	}
	defer for s in cs do delete(s)
	err = check(CreateCubemap(cs[0], cs[1], cs[2], cs[3], cs[4], cs[5], &cubemap))
	return
}

// Waits for the GPU to finish the frames in flight. Scenes using it go back to having no skybox.
destroy_cubemap :: proc "contextless" (cubemap: Cubemap) -> Error_Code { return check(DestroyCubemap(cubemap)) }

// The bound scene's sky (3D only). The empty handle Cubemap{} removes it.
set_skybox :: proc "contextless" (cubemap: Cubemap) -> Error_Code { return check(SetSkybox(cubemap)) }

// ---------------------------------------------------------------------------
// Camera. Angles are in radians.
// ---------------------------------------------------------------------------

set_camera_position :: proc "contextless" (position: Vec3) -> Error_Code { return check(SetCameraPosition(position)) }

get_camera_position :: proc "contextless" () -> (position: Vec3, err: Error_Code) {
	err = check(GetCameraPosition(&position))
	return
}

move_camera :: proc "contextless" (offset: Vec3) -> Error_Code { return check(MoveCamera(offset)) }
move_camera_relative :: proc "contextless" (offset: Vec3) -> Error_Code { return check(MoveCameraRelative(offset)) }
set_camera_rotation :: proc "contextless" (yaw, pitch: f32) -> Error_Code { return check(SetCameraRotation(yaw, pitch)) }

rotate_camera :: proc "contextless" (yaw_delta, pitch_delta: f32) -> Error_Code {
	return check(RotateCamera(yaw_delta, pitch_delta))
}

camera_look_at :: proc "contextless" (target: Vec3) -> Error_Code { return check(CameraLookAt(target)) }

get_camera_forward :: proc "contextless" () -> (forward: Vec3, err: Error_Code) {
	err = check(GetCameraForward(&forward))
	return
}

get_view_matrix :: proc "contextless" () -> (view: Mat4, err: Error_Code) {
	err = check(GetViewMatrix(&view))
	return
}

// Vulkan conventions: clip-space +Y down, reverse-Z with an infinite far plane. See renderer.h.
get_projection_matrix :: proc "contextless" () -> (projection: Mat4, err: Error_Code) {
	err = check(GetProjectionMatrix(&projection))
	return
}

// The nearest visible instance of the bound scene under a pixel (get_mouse_position units). ok is false when
// nothing is hit (hit is then {}). Render_3D only.
pick_instance :: proc "contextless" (window_pos: Vec2) -> (hit: Ray_Hit, ok: bool, err: Error_Code) {
	err = check(PickInstance(window_pos, &ok, &hit))
	if err != .Ok || !ok do return {}, false, err
	return
}

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------

// Frames per second, averaged over the last half second.
get_fps :: proc "contextless" () -> (fps: f32, err: Error_Code) {
	err = check(GetFPS(&fps))
	return
}

// Seconds between the last two presented frames.
get_delta_time :: proc "contextless" () -> (delta_time: f32, err: Error_Code) {
	err = check(GetDeltaTime(&delta_time))
	return
}

// ---------------------------------------------------------------------------
// Input. Main thread only. Pressed / released are true for the one frame after the poll_events call
// that saw it happen; the deltas cover the time between the last two poll_events calls (see renderer.h).
// ---------------------------------------------------------------------------

is_mouse_button_down :: proc "contextless" (button: Mouse_Button) -> (down: bool, err: Error_Code) {
	err = check(IsMouseButtonDown(button, &down))
	return
}

is_mouse_button_pressed :: proc "contextless" (button: Mouse_Button) -> (pressed: bool, err: Error_Code) {
	err = check(IsMouseButtonPressed(button, &pressed))
	return
}

is_mouse_button_released :: proc "contextless" (button: Mouse_Button) -> (released: bool, err: Error_Code) {
	err = check(IsMouseButtonReleased(button, &released))
	return
}

// How far the cursor moved, in pixels. +x is right, +y is down.
get_mouse_delta :: proc "contextless" () -> (delta: Vec2, err: Error_Code) {
	err = check(GetMouseDelta(&delta))
	return
}

// Scroll wheel steps. Positive means scrolling up / away from you.
get_scroll_delta :: proc "contextless" () -> (delta: f32, err: Error_Code) {
	err = check(GetScrollDelta(&delta))
	return
}

// Pixels from the window's top-left corner, as of the last poll_events call.
get_mouse_position :: proc "contextless" () -> (position: Vec2, err: Error_Code) {
	err = check(GetMousePosition(&position))
	return
}

set_cursor_mode :: proc "contextless" (mode: Cursor_Mode) -> Error_Code { return check(SetCursorMode(mode)) }

is_key_down :: proc "contextless" (key: Key) -> (down: bool, err: Error_Code) {
	err = check(IsKeyDown(key, &down))
	return
}

is_key_pressed :: proc "contextless" (key: Key) -> (pressed: bool, err: Error_Code) {
	err = check(IsKeyPressed(key, &pressed))
	return
}

is_key_released :: proc "contextless" (key: Key) -> (released: bool, err: Error_Code) {
	err = check(IsKeyReleased(key, &released))
	return
}
