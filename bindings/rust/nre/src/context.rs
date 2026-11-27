//! [`Context`]: the API functions as methods, for the typed layer.

use std::fmt;
use std::marker::PhantomData;
use std::path::Path;

use crate::{
    Cubemap, CursorMode, Instance, Key, Light, Mat4, Material, Model, MouseButton, Owned, Pipeline, RayHit, Resource,
    Result, Scene, SceneLighting, Shader, ShaderType, Texture, Transform, Vec2, Vec3, Vertex,
};

/// The renderer's API as methods: everything except starting frames, the draw calls and shutting down. Each
/// method is the free function of the same name (`create_model` is [`crate::create_model`]); see those and
/// renderer.h for what they do.
///
/// You don't create one: [`Renderer`](crate::Renderer) and [`Frame`](crate::Frame) deref to it, and
/// [`Renderer::split`](crate::Renderer::split) hands one out. Take `&Context` in your own helpers (a camera
/// controller, ...) to use them both outside and during a frame.
///
/// Every method takes `&self`: none of them can break the frame in progress. They change what the renderer will
/// draw (the scene, instances, camera, ...) or read input, and the renderer handles that changing mid-frame (a
/// destroyed scene or model gives an empty frame or a skipped draw). What can't happen during a frame (starting
/// another one, shutting down) takes `&mut self` / `self` on [`Renderer`](crate::Renderer), and the borrow
/// checker keeps it from overlapping a [`Frame`](crate::Frame).
///
/// Not `Send` or `Sync`: it stays on the thread that created the renderer.
pub struct Context {
    manual_frame_presentation_and_submission: bool,
    _not_send: PhantomData<*const ()>,
}

impl Context {
    pub(crate) fn new(manual_frame_presentation_and_submission: bool) -> Self {
        Self { manual_frame_presentation_and_submission, _not_send: PhantomData }
    }

    /// Whether the renderer was created with `manual_frame_presentation_and_submission`, in which case ending a
    /// [`Frame`](crate::Frame) also submits and presents it.
    pub fn manual_frame_presentation_and_submission(&self) -> bool {
        self.manual_frame_presentation_and_submission
    }

    /// Takes ownership of a handle: the returned [`Owned`] destroys it when dropped and can't outlive the
    /// renderer. To keep owned handles across frames, borrow them from the context [`Renderer::split`]
    /// returns, not from the renderer itself.
    ///
    /// [`Renderer::split`]: crate::Renderer::split
    pub fn own<T: Resource>(&self, handle: T) -> Owned<'_, T> {
        Owned::new(handle)
    }

    // ----- Window and events -----

    /// [`crate::window_should_close`].
    pub fn window_should_close(&self) -> Result<bool> {
        crate::window_should_close()
    }

    /// [`crate::poll_events`]: processes window and input events.
    pub fn poll_events(&self) -> Result<()> {
        crate::poll_events()
    }

    // ----- Shaders and pipelines -----

    /// [`crate::create_shader`]: loads a compiled SPIR-V file.
    pub fn create_shader(&self, shader_path: impl AsRef<Path>, shader_type: ShaderType) -> Result<Shader> {
        crate::create_shader(shader_path, shader_type)
    }

    /// [`crate::destroy_shader`].
    pub fn destroy_shader(&self, shader: Shader) -> Result<()> {
        crate::destroy_shader(shader)
    }

    /// [`crate::create_pipeline`].
    pub fn create_pipeline(&self, vertex_shader: Shader, fragment_shader: Shader) -> Result<Pipeline> {
        crate::create_pipeline(vertex_shader, fragment_shader)
    }

    /// [`crate::destroy_pipeline`].
    pub fn destroy_pipeline(&self, pipeline: Pipeline) -> Result<()> {
        crate::destroy_pipeline(pipeline)
    }

    /// [`crate::bind_pipeline`]. Frames use the pipeline bound when they start.
    pub fn bind_pipeline(&self, pipeline: Pipeline) -> Result<()> {
        crate::bind_pipeline(pipeline)
    }

    // ----- Models and instances -----

    /// [`crate::create_model`]: loads a model file (glTF/GLB, OBJ, ...).
    pub fn create_model(&self, model_path: impl AsRef<Path>) -> Result<Model> {
        crate::create_model(model_path)
    }

    /// [`crate::create_mesh_model`]: a model from your own triangles. `None` as material uses the default one.
    pub fn create_mesh_model(&self, vertices: &[Vertex], indices: &[u32], material: Option<Material>) -> Result<Model> {
        crate::create_mesh_model(vertices, indices, material)
    }

    /// [`crate::destroy_model`]. Instances of a destroyed model stay valid but draw nothing.
    pub fn destroy_model(&self, model: Model) -> Result<()> {
        crate::destroy_model(model)
    }

    /// [`crate::create_instance`].
    pub fn create_instance(&self, model: Model) -> Result<Instance> {
        crate::create_instance(model)
    }

    /// [`crate::destroy_instance`]. Also removes it from every scene it's in.
    pub fn destroy_instance(&self, instance: Instance) -> Result<()> {
        crate::destroy_instance(instance)
    }

    /// [`crate::set_instance_transform`].
    pub fn set_instance_transform(&self, instance: Instance, transform: &Transform) -> Result<()> {
        crate::set_instance_transform(instance, transform)
    }

    /// [`crate::get_instance_transform`].
    pub fn get_instance_transform(&self, instance: Instance) -> Result<Transform> {
        crate::get_instance_transform(instance)
    }

    /// [`crate::get_instance_matrix`].
    pub fn get_instance_matrix(&self, instance: Instance) -> Result<Mat4> {
        crate::get_instance_matrix(instance)
    }

    /// [`crate::translate_instance`].
    pub fn translate_instance(&self, instance: Instance, offset: impl Into<Vec3>) -> Result<()> {
        crate::translate_instance(instance, offset)
    }

    /// [`crate::rotate_instance`]. `angle` is in radians.
    pub fn rotate_instance(&self, instance: Instance, angle: f32, axis: impl Into<Vec3>) -> Result<()> {
        crate::rotate_instance(instance, angle, axis)
    }

    /// [`crate::scale_instance`].
    pub fn scale_instance(&self, instance: Instance, factor: impl Into<Vec3>) -> Result<()> {
        crate::scale_instance(instance, factor)
    }

    /// [`crate::set_visibility`]. A hidden instance stays in its scenes but isn't drawn.
    pub fn set_visibility(&self, instance: Instance, visible: bool) -> Result<()> {
        crate::set_visibility(instance, visible)
    }

    /// [`crate::set_casts_shadows`]. It still receives shadows.
    pub fn set_casts_shadows(&self, instance: Instance, casts_shadows: bool) -> Result<()> {
        crate::set_casts_shadows(instance, casts_shadows)
    }

    // ----- Scenes -----

    /// [`crate::create_scene`].
    pub fn create_scene(&self) -> Result<Scene> {
        crate::create_scene()
    }

    /// [`crate::destroy_scene`].
    pub fn destroy_scene(&self, scene: Scene) -> Result<()> {
        crate::destroy_scene(scene)
    }

    /// [`crate::bind_scene`]. Frames use the scene bound when they start.
    pub fn bind_scene(&self, scene: Scene) -> Result<()> {
        crate::bind_scene(scene)
    }

    /// [`crate::get_bound_scene`].
    pub fn get_bound_scene(&self) -> Result<Scene> {
        crate::get_bound_scene()
    }

    /// [`crate::add_instance_to_scene`]: adds to the bound scene.
    pub fn add_instance_to_scene(&self, instance: Instance) -> Result<()> {
        crate::add_instance_to_scene(instance)
    }

    /// [`crate::remove_instance_from_scene`].
    pub fn remove_instance_from_scene(&self, instance: Instance) -> Result<()> {
        crate::remove_instance_from_scene(instance)
    }

    // ----- Textures and materials -----

    /// [`crate::create_texture`]: loads an image file as a color (sRGB) texture.
    pub fn create_texture(&self, path: impl AsRef<Path>) -> Result<Texture> {
        crate::create_texture(path)
    }

    /// [`crate::create_texture_with`]: `srgb = false` for data maps (normal, metallic-roughness).
    pub fn create_texture_with(&self, path: impl AsRef<Path>, srgb: bool) -> Result<Texture> {
        crate::create_texture_with(path, srgb)
    }

    /// [`crate::destroy_texture`]. Give the materials that use it another texture first.
    pub fn destroy_texture(&self, texture: Texture) -> Result<()> {
        crate::destroy_texture(texture)
    }

    /// [`crate::create_material`]. `None` as base texture uses the default texture.
    pub fn create_material(
        &self,
        base_texture: Option<Texture>,
        metallic_texture: Option<Texture>,
        normal_texture: Option<Texture>,
    ) -> Result<Material> {
        crate::create_material(base_texture, metallic_texture, normal_texture)
    }

    /// [`crate::destroy_material`]. Models still using it fall back to the default material.
    pub fn destroy_material(&self, material: Material) -> Result<()> {
        crate::destroy_material(material)
    }

    /// [`crate::set_material_base_texture`]. Waits for the GPU: don't call it every frame.
    pub fn set_material_base_texture(&self, material: Material, texture: Option<Texture>) -> Result<()> {
        crate::set_material_base_texture(material, texture)
    }

    /// [`crate::get_model_materials`].
    pub fn get_model_materials(&self, model: Model) -> Result<Vec<Material>> {
        crate::get_model_materials(model)
    }

    /// [`crate::set_model_material`].
    pub fn set_model_material(&self, model: Model, material: Material) -> Result<()> {
        crate::set_model_material(model, material)
    }

    /// [`crate::replace_model_material`].
    pub fn replace_model_material(&self, model: Model, index: u32, material: Material) -> Result<()> {
        crate::replace_model_material(model, index, material)
    }

    // ----- Lights and the sun -----

    /// [`crate::create_directional_light`]: white, intensity 3.
    pub fn create_directional_light(&self, direction: impl Into<Vec3>) -> Result<Light> {
        crate::create_directional_light(direction)
    }

    /// [`crate::create_directional_light_with`].
    pub fn create_directional_light_with(
        &self,
        direction: impl Into<Vec3>,
        color: impl Into<Vec3>,
        intensity: f32,
    ) -> Result<Light> {
        crate::create_directional_light_with(direction, color, intensity)
    }

    /// [`crate::create_point_light`]: white, intensity 10.
    pub fn create_point_light(&self, position: impl Into<Vec3>) -> Result<Light> {
        crate::create_point_light(position)
    }

    /// [`crate::create_point_light_with`].
    pub fn create_point_light_with(
        &self,
        position: impl Into<Vec3>,
        color: impl Into<Vec3>,
        intensity: f32,
    ) -> Result<Light> {
        crate::create_point_light_with(position, color, intensity)
    }

    /// [`crate::destroy_light`]. Also removes it from every scene it's in.
    pub fn destroy_light(&self, light: Light) -> Result<()> {
        crate::destroy_light(light)
    }

    /// [`crate::add_light_to_scene`]: adds to the bound scene.
    pub fn add_light_to_scene(&self, light: Light) -> Result<()> {
        crate::add_light_to_scene(light)
    }

    /// [`crate::remove_light_from_scene`].
    pub fn remove_light_from_scene(&self, light: Light) -> Result<()> {
        crate::remove_light_from_scene(light)
    }

    /// [`crate::set_light_position`]. Only used by point lights.
    pub fn set_light_position(&self, light: Light, position: impl Into<Vec3>) -> Result<()> {
        crate::set_light_position(light, position)
    }

    /// [`crate::set_light_direction`]. Only used by directional lights.
    pub fn set_light_direction(&self, light: Light, direction: impl Into<Vec3>) -> Result<()> {
        crate::set_light_direction(light, direction)
    }

    /// [`crate::set_light_color`].
    pub fn set_light_color(&self, light: Light, color: impl Into<Vec3>, intensity: f32) -> Result<()> {
        crate::set_light_color(light, color, intensity)
    }

    /// [`crate::set_sun`]: white, intensity 3.
    pub fn set_sun(&self, direction: impl Into<Vec3>) -> Result<()> {
        crate::set_sun(direction)
    }

    /// [`crate::set_sun_with`]: the bound scene's sun.
    pub fn set_sun_with(&self, direction: impl Into<Vec3>, color: impl Into<Vec3>, intensity: f32) -> Result<()> {
        crate::set_sun_with(direction, color, intensity)
    }

    /// [`crate::set_sun_direction`]. Keeps the color and intensity.
    pub fn set_sun_direction(&self, direction: impl Into<Vec3>) -> Result<()> {
        crate::set_sun_direction(direction)
    }

    /// [`crate::get_sun_direction`].
    pub fn get_sun_direction(&self) -> Result<Vec3> {
        crate::get_sun_direction()
    }

    // ----- Scene lighting, shadows, haze, exposure and sky colour (bound scene) -----

    /// [`crate::set_scene_lighting`]. Also resets shadow distance, haze and the sky to the mode's defaults.
    pub fn set_scene_lighting(&self, lighting: SceneLighting) -> Result<()> {
        crate::set_scene_lighting(lighting)
    }

    /// [`crate::get_scene_lighting`].
    pub fn get_scene_lighting(&self) -> Result<SceneLighting> {
        crate::get_scene_lighting()
    }

    /// [`crate::set_shadow_distance`]: metres, 0 = no sun shadows.
    pub fn set_shadow_distance(&self, meters: f32) -> Result<()> {
        crate::set_shadow_distance(meters)
    }

    /// [`crate::set_haze`]: a 300 m thick haze layer.
    pub fn set_haze(&self, visibility: f32) -> Result<()> {
        crate::set_haze(visibility)
    }

    /// [`crate::set_haze_with`].
    pub fn set_haze_with(&self, visibility: f32, height: f32) -> Result<()> {
        crate::set_haze_with(visibility, height)
    }

    /// [`crate::set_exposure`]: 2 is twice as bright.
    pub fn set_exposure(&self, exposure: f32) -> Result<()> {
        crate::set_exposure(exposure)
    }

    /// [`crate::set_sky_color`]: the background without a skybox (Small).
    pub fn set_sky_color(&self, color: impl Into<Vec3>) -> Result<()> {
        crate::set_sky_color(color)
    }

    // ----- Cubemaps and the skybox -----

    /// [`crate::create_cubemap`]: six faces in the OpenGL order (+X, -X, +Y, -Y, +Z, -Z).
    pub fn create_cubemap(
        &self,
        right: impl AsRef<Path>,
        left: impl AsRef<Path>,
        top: impl AsRef<Path>,
        bottom: impl AsRef<Path>,
        front: impl AsRef<Path>,
        back: impl AsRef<Path>,
    ) -> Result<Cubemap> {
        crate::create_cubemap(right, left, top, bottom, front, back)
    }

    /// [`crate::destroy_cubemap`]. Scenes using it go back to having no skybox.
    pub fn destroy_cubemap(&self, cubemap: Cubemap) -> Result<()> {
        crate::destroy_cubemap(cubemap)
    }

    /// [`crate::set_skybox`]: the bound scene's sky. `None` removes it.
    pub fn set_skybox(&self, cubemap: Option<Cubemap>) -> Result<()> {
        crate::set_skybox(cubemap)
    }

    // ----- Camera (radians). Frames read it when they start. -----

    /// [`crate::set_camera_position`].
    pub fn set_camera_position(&self, position: impl Into<Vec3>) -> Result<()> {
        crate::set_camera_position(position)
    }

    /// [`crate::get_camera_position`].
    pub fn get_camera_position(&self) -> Result<Vec3> {
        crate::get_camera_position()
    }

    /// [`crate::move_camera`].
    pub fn move_camera(&self, offset: impl Into<Vec3>) -> Result<()> {
        crate::move_camera(offset)
    }

    /// [`crate::move_camera_relative`].
    pub fn move_camera_relative(&self, offset: impl Into<Vec3>) -> Result<()> {
        crate::move_camera_relative(offset)
    }

    /// [`crate::set_camera_rotation`].
    pub fn set_camera_rotation(&self, yaw: f32, pitch: f32) -> Result<()> {
        crate::set_camera_rotation(yaw, pitch)
    }

    /// [`crate::rotate_camera`].
    pub fn rotate_camera(&self, yaw_delta: f32, pitch_delta: f32) -> Result<()> {
        crate::rotate_camera(yaw_delta, pitch_delta)
    }

    /// [`crate::camera_look_at`].
    pub fn camera_look_at(&self, target: impl Into<Vec3>) -> Result<()> {
        crate::camera_look_at(target)
    }

    /// [`crate::get_camera_forward`].
    pub fn get_camera_forward(&self) -> Result<Vec3> {
        crate::get_camera_forward()
    }

    /// [`crate::get_view_matrix`].
    pub fn get_view_matrix(&self) -> Result<Mat4> {
        crate::get_view_matrix()
    }

    /// [`crate::get_projection_matrix`]: Vulkan conventions (+Y down, reverse-Z, infinite far plane).
    pub fn get_projection_matrix(&self) -> Result<Mat4> {
        crate::get_projection_matrix()
    }

    /// [`crate::pick_instance`]: the nearest visible instance of the bound scene under a pixel, or `None`.
    pub fn pick_instance(&self, window_pos: impl Into<Vec2>) -> Result<Option<RayHit>> {
        crate::pick_instance(window_pos)
    }

    // ----- Timing -----

    /// [`crate::get_fps`]: averaged over the last half second.
    pub fn get_fps(&self) -> Result<f32> {
        crate::get_fps()
    }

    /// [`crate::get_delta_time`]: seconds between the last two presented frames.
    pub fn get_delta_time(&self) -> Result<f32> {
        crate::get_delta_time()
    }

    // ----- Input (as of the last poll_events) -----

    /// [`crate::is_mouse_button_down`].
    pub fn is_mouse_button_down(&self, button: MouseButton) -> Result<bool> {
        crate::is_mouse_button_down(button)
    }

    /// [`crate::is_mouse_button_pressed`]: went down during the last `poll_events`.
    pub fn is_mouse_button_pressed(&self, button: MouseButton) -> Result<bool> {
        crate::is_mouse_button_pressed(button)
    }

    /// [`crate::is_mouse_button_released`]: went up during the last `poll_events`.
    pub fn is_mouse_button_released(&self, button: MouseButton) -> Result<bool> {
        crate::is_mouse_button_released(button)
    }

    /// [`crate::get_mouse_delta`]: in pixels, +x right, +y down.
    pub fn get_mouse_delta(&self) -> Result<Vec2> {
        crate::get_mouse_delta()
    }

    /// [`crate::get_scroll_delta`]: positive is up / away from you.
    pub fn get_scroll_delta(&self) -> Result<f32> {
        crate::get_scroll_delta()
    }

    /// [`crate::get_mouse_position`]: pixels from the window's top-left corner.
    pub fn get_mouse_position(&self) -> Result<Vec2> {
        crate::get_mouse_position()
    }

    /// [`crate::set_cursor_mode`].
    pub fn set_cursor_mode(&self, mode: CursorMode) -> Result<()> {
        crate::set_cursor_mode(mode)
    }

    /// [`crate::is_key_down`].
    pub fn is_key_down(&self, key: Key) -> Result<bool> {
        crate::is_key_down(key)
    }

    /// [`crate::is_key_pressed`]: went down during the last `poll_events` (no repeat).
    pub fn is_key_pressed(&self, key: Key) -> Result<bool> {
        crate::is_key_pressed(key)
    }

    /// [`crate::is_key_released`]: went up during the last `poll_events`.
    pub fn is_key_released(&self, key: Key) -> Result<bool> {
        crate::is_key_released(key)
    }
}

impl fmt::Debug for Context {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Context")
            .field("manual_frame_presentation_and_submission", &self.manual_frame_presentation_and_submission)
            .finish()
    }
}
