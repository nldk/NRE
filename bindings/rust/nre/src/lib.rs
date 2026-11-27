//! Safe Rust bindings for the NRE renderer, over its C API (libnre_c, `include/nre.h`).
//!
//! The functions mirror the C++ API in renderer.h (`NRE::createModel` is [`create_model`]); see renderer.h for
//! what each one does. Every call returns a [`Result`]; an [`Error`] carries the [`ErrorCode`] and the library's
//! message for that failure.
//!
//! - **Threads:** GLFW needs the main thread, so call [`init`], [`poll_events`], the frame functions and the input
//!   functions from the thread that runs `main` (not from a `cargo test` worker thread, for instance).
//! - **Handles** ([`Model`], [`Instance`], ...) are `Copy` values destroyed explicitly with `destroy_*`, like in C++.
//!   Dropping one does nothing.
//! - **Version check:** before its first call into the library, every wrapper runs [`check_version`] once
//!   (cached in a `OnceLock`), so a libnre_c that isn't 0.3.x makes every call fail with
//!   [`ErrorCode::LibraryLoadFailed`] instead of misbehaving. You can call it yourself to fail early.
//! - **Defaults:** Rust has no default arguments, so a function with defaulted C++ parameters comes in two forms:
//!   the short one uses renderer.h's defaults ([`set_render_config`], [`create_directional_light`], ...) and the
//!   `_with` one takes every parameter.
//! - **Strings and paths** are passed as NUL-terminated UTF-8. One containing a NUL byte (or a non-UTF-8 path on
//!   Windows) fails with [`ErrorCode::Unknown`] before reaching the library.
//!
//! ## Two layers
//!
//! - **Low level:** the free functions in this crate root, one per C++ function. Nothing stops misuse (a draw
//!   call in Scene mode, a frame never ended, a call before `set_render_config`, which aborts the process); the
//!   renderer's Debug build reports it at run time.
//! - **Typed:** [`Renderer<M>`](Renderer), with the API mode as a type ([`SceneMode`] / [`DrawMode`]). It does
//!   `set_render_config` + `init` when created and `shutdown` when dropped, there can only be one, frames are
//!   guards ([`Frame`]) that end themselves, draw calls only exist on `Frame<DrawMode>`, and the borrow checker
//!   keeps frames from overlapping and the renderer from being shut down during one. [`Owned`] handles destroy
//!   themselves. Every API function is a method of [`Context`], which `Renderer` and `Frame` deref to.
//!   `use nre::prelude::*` brings in its types.
//!
//! ```no_run
//! use nre::prelude::*;
//!
//! fn main() -> nre::Result<()> {
//!     let mut nre = Renderer::<DrawMode>::new(800, 600, "hello")?;
//!     let car = nre.create_instance(nre.create_model("police.glb")?)?;
//!     let vertex = nre.create_shader("vertex.vert.spv", ShaderType::Vertex)?;
//!     let fragment = nre.create_shader("fragment.frag.spv", ShaderType::Fragment)?;
//!     nre.bind_pipeline(nre.create_pipeline(vertex, fragment)?)?;
//!     while !nre.window_should_close()? {
//!         nre.poll_events()?;
//!         let frame = nre.frame()?;
//!         frame.draw_instance(car)?;
//!         frame.rotate_instance(car, 0.01, Vec3::Y)?; // Context methods work during the frame
//!     }
//!     Ok(())
//! }
//! ```
//!
//! The same with the low-level functions, in Scene mode:
//!
//! ```no_run
//! fn main() -> nre::Result<()> {
//!     nre::set_render_config(800, 600, "hello")?;
//!     nre::init()?;
//!     let model = nre::create_model("box.glb")?;
//!     let instance = nre::create_instance(model)?;
//!     nre::create_scene()?;
//!     nre::add_instance_to_scene(instance)?;
//!     while !nre::window_should_close()? {
//!         nre::poll_events()?;
//!         nre::render_frame()?;
//!     }
//!     nre::shutdown()
//! }
//! ```

mod context;
mod enums;
mod error;
mod handles;
mod interop;
mod math;
mod owned;
mod renderer;

use std::ffi::{CString, c_char};
use std::path::Path;
use std::sync::OnceLock;

/// The raw C API, for anything these bindings don't cover.
pub use nre_sys as sys;

pub use enums::{
    ApiMode, CursorMode, ErrorCode, Key, MouseButton, RenderErrorPolicy, RenderMode, SceneLighting, ShaderType,
};
pub use error::{Error, Result};
pub use handles::{Cubemap, Instance, Light, Material, Model, Pipeline, Scene, Shader, Texture};
pub use math::{Mat4, Quat, RayHit, Transform, Vec2, Vec3, Vertex};

pub use context::Context;
pub use owned::{Owned, Resource};
pub use renderer::{DrawMode, Frame, Frames, Mode, Renderer, SceneMode};

/// The typed layer's types, the handles, the math types and the enums its methods take:
/// `use nre::prelude::*;`. Not [`Result`] / [`Error`], which would shadow `std`'s: write `nre::Result`.
pub mod prelude {
    pub use crate::{
        Context, Cubemap, CursorMode, DrawMode, ErrorCode, Frame, Frames, Instance, Key, Light, Mat4, Material, Mode,
        Model, MouseButton, Owned, Pipeline, Quat, RayHit, RenderErrorPolicy, RenderMode, Renderer, Resource, Scene,
        SceneLighting, SceneMode, Shader, ShaderType, Texture, Transform, Vec2, Vec3, Vertex,
    };
}

use error::copy_c_str;
use sys::{NRE_OK, NreErrorCode};

/// The libnre_c `major.minor` these bindings are written for.
pub const ABI_VERSION: (u32, u32) = (0, 3);

// ---------------------------------------------------------------------------
// Call helpers
// ---------------------------------------------------------------------------

/// Runs one C call after the version check, turning a non-`NRE_OK` return into an [`Error`].
fn call(f: impl FnOnce() -> NreErrorCode) -> Result<()> {
    check_version()?;
    match f() {
        NRE_OK => Ok(()),
        code => Err(Error::from_last_call(code)),
    }
}

/// [`call`] for a function with one out-parameter, which becomes the return value.
fn get<T: Default>(f: impl FnOnce(*mut T) -> NreErrorCode) -> Result<T> {
    let mut out = T::default();
    call(|| f(&mut out))?;
    Ok(out)
}

fn c_string(s: &str) -> Result<CString> {
    CString::new(s).map_err(|_| Error::new(ErrorCode::Unknown, format!("string contains a NUL byte: {s:?}")))
}

fn c_path(path: &Path) -> Result<CString> {
    #[cfg(unix)]
    let bytes = std::os::unix::ffi::OsStrExt::as_bytes(path.as_os_str());
    #[cfg(not(unix))]
    let bytes = path
        .to_str()
        .ok_or_else(|| Error::new(ErrorCode::Unknown, format!("path is not valid UTF-8: {path:?}")))?
        .as_bytes();
    CString::new(bytes).map_err(|_| Error::new(ErrorCode::Unknown, format!("path contains a NUL byte: {path:?}")))
}

// ---------------------------------------------------------------------------
// Errors and version
// ---------------------------------------------------------------------------

/// The message of the most recent failed call on this thread ("" if none failed yet). [`Error`] already
/// carries it; this is for calls made through [`sys`].
pub fn get_last_error_message() -> String {
    // SAFETY: returns a NUL-terminated string valid until the next failed call on this thread; copied right away.
    unsafe { copy_c_str(sys::nreGetLastErrorMessage()) }
}

/// The library's name for a code: "FileNotFound", ... (renderer.h's `toString`).
pub fn error_code_to_string(code: ErrorCode) -> &'static str {
    // SAFETY: any NreErrorCode value is accepted; the result is a static string.
    let ptr: *const c_char = unsafe { sys::nreErrorCodeToString(code.to_raw()) };
    if ptr.is_null() {
        return "";
    }
    // SAFETY: a NUL-terminated string literal inside the library, valid for as long as the library is loaded,
    // which (linked at load time) is the whole program.
    unsafe { std::ffi::CStr::from_ptr(ptr) }.to_str().unwrap_or("")
}

/// The loaded library's `(major, minor, patch)` version.
pub fn get_version() -> (u32, u32, u32) {
    let (mut major, mut minor, mut patch) = (0, 0, 0);
    // SAFETY: three valid out-pointers.
    unsafe { sys::nreGetVersion(&mut major, &mut minor, &mut patch) };
    (major, minor, patch)
}

/// Checks that the loaded libnre_c is version [`ABI_VERSION`]`.x`, failing with [`ErrorCode::LibraryLoadFailed`]
/// if not. Runs once; every other wrapper calls it (and gets the cached answer) before calling into the library.
pub fn check_version() -> Result<()> {
    static RESULT: OnceLock<Result<()>> = OnceLock::new();
    RESULT
        .get_or_init(|| {
            let (major, minor, patch) = get_version();
            if (major, minor) == ABI_VERSION {
                Ok(())
            } else {
                Err(Error::new(
                    ErrorCode::LibraryLoadFailed,
                    format!(
                        "libnre_c is version {major}.{minor}.{patch}, but these bindings need {}.{}.x",
                        ABI_VERSION.0, ABI_VERSION.1
                    ),
                ))
            }
        })
        .clone()
}

// ---------------------------------------------------------------------------
// Setup and frames
// ---------------------------------------------------------------------------

/// [`set_render_config_with`] with renderer.h's defaults: 3D, [`ApiMode::Scene`], [`RenderErrorPolicy::Return`],
/// automatic presentation and submission, a resizable window with the FPS in its title. Call before [`init`].
pub fn set_render_config(width: i32, height: i32, title: &str) -> Result<()> {
    set_render_config_with(
        width,
        height,
        title,
        RenderMode::default(),
        ApiMode::default(),
        RenderErrorPolicy::default(),
        false,
        true,
        true,
    )
}

/// Sets the renderer and window configuration (renderer.h's `RenderConfig` and `WindowConfig`). Call before [`init`].
#[allow(clippy::too_many_arguments)]
pub fn set_render_config_with(
    width: i32,
    height: i32,
    title: &str,
    mode: RenderMode,
    api_mode: ApiMode,
    error_policy: RenderErrorPolicy,
    manual_frame_presentation_and_submission: bool,
    resizable: bool,
    show_fps_in_title: bool,
) -> Result<()> {
    let title = c_string(title)?;
    call(|| unsafe {
        sys::nreSetRenderConfig(
            mode.to_raw(),
            api_mode.to_raw(),
            error_policy.to_raw(),
            manual_frame_presentation_and_submission,
            width,
            height,
            title.as_ptr(),
            resizable,
            show_fps_in_title,
        )
    })
}

/// Opens the window and sets up Vulkan. Main thread only (GLFW).
pub fn init() -> Result<()> {
    call(|| unsafe { sys::nreInit() })
}

pub fn shutdown() -> Result<()> {
    call(|| unsafe { sys::nreShutdown() })
}

/// [`ApiMode::Scene`]: draws the bound scene and (unless manual presentation and submission) submits and presents it.
/// [`ApiMode::FunctionDraw`]: starts the frame, raylib style: `render_frame()`, then [`draw_model`] /
/// [`draw_instance`] calls, then [`end_frame`]. The camera, lights and pipeline come from the bound scene.
pub fn render_frame() -> Result<()> {
    call(|| unsafe { sys::nreRenderFrame() })
}

/// [`ApiMode::FunctionDraw`] only: ends the frame [`render_frame`] started and (unless manual presentation and
/// submission) submits and presents it.
pub fn end_frame() -> Result<()> {
    call(|| unsafe { sys::nreEndFrame() })
}

pub fn present_frame() -> Result<()> {
    call(|| unsafe { sys::nrePresentFrame() })
}

pub fn submit_frame() -> Result<()> {
    call(|| unsafe { sys::nreSubmitFrame() })
}

pub fn window_should_close() -> Result<bool> {
    get(|out| unsafe { sys::nreWindowShouldClose(out) })
}

/// Processes window and input events. Main thread only (GLFW).
pub fn poll_events() -> Result<()> {
    call(|| unsafe { sys::nrePollEvents() })
}

// ---------------------------------------------------------------------------
// Shaders and pipelines
// ---------------------------------------------------------------------------

/// Loads a compiled SPIR-V file.
pub fn create_shader(shader_path: impl AsRef<Path>, shader_type: ShaderType) -> Result<Shader> {
    let path = c_path(shader_path.as_ref())?;
    get(|out| unsafe { sys::nreCreateShader(path.as_ptr(), shader_type.to_raw(), out) }).map(Shader)
}

pub fn destroy_shader(shader: Shader) -> Result<()> {
    call(|| unsafe { sys::nreDestroyShader(shader.0) })
}

pub fn create_pipeline(vertex_shader: Shader, fragment_shader: Shader) -> Result<Pipeline> {
    get(|out| unsafe { sys::nreCreatePipeline(vertex_shader.0, fragment_shader.0, out) }).map(Pipeline)
}

pub fn destroy_pipeline(pipeline: Pipeline) -> Result<()> {
    call(|| unsafe { sys::nreDestroyPipeline(pipeline.0) })
}

pub fn bind_pipeline(pipeline: Pipeline) -> Result<()> {
    call(|| unsafe { sys::nreBindPipeline(pipeline.0) })
}

// ---------------------------------------------------------------------------
// Models and instances
// ---------------------------------------------------------------------------

/// Loads a model file (glTF/GLB, OBJ, ...). It isn't drawn by itself: create [`Instance`]s of it.
pub fn create_model(model_path: impl AsRef<Path>) -> Result<Model> {
    let path = c_path(model_path.as_ref())?;
    get(|out| unsafe { sys::nreCreateModel(path.as_ptr(), out) }).map(Model)
}

/// A model from your own geometry (terrain, water, editor meshes): a triangle list with counter-clockwise front
/// faces, `indices` into `vertices`. Vertices whose normal is zero get one computed from the triangles that use
/// them. `None` as material uses the default one. Draw it through [`Instance`]s like a loaded model. The slices
/// are passed to C as they are (no copy here) and copied by the renderer.
///
/// More than `u32::MAX` vertices or indices fails with [`ErrorCode::InvalidArgument`] before reaching the library.
pub fn create_mesh_model(vertices: &[Vertex], indices: &[u32], material: Option<Material>) -> Result<Model> {
    let count = |len: usize, what: &str| {
        u32::try_from(len).map_err(|_| Error::new(ErrorCode::InvalidArgument, format!("too many {what}: {len}")))
    };
    let (vertex_count, index_count) = (count(vertices.len(), "vertices")?, count(indices.len(), "indices")?);
    // Vertex is repr(C) with NreVertex's layout (checked at compile time in math.rs).
    let vertex_ptr = vertices.as_ptr().cast::<sys::NreVertex>();
    let material = material.unwrap_or(Material::NONE).0;
    get(|out| unsafe {
        sys::nreCreateMeshModel(vertex_ptr, vertex_count, indices.as_ptr(), index_count, material, out)
    })
    .map(Model)
}

/// Instances of a destroyed model stay valid but draw nothing.
pub fn destroy_model(model: Model) -> Result<()> {
    call(|| unsafe { sys::nreDestroyModel(model.0) })
}

pub fn create_instance(model: Model) -> Result<Instance> {
    get(|out| unsafe { sys::nreCreateInstance(model.0, out) }).map(Instance)
}

/// Also removes it from every scene it's in.
pub fn destroy_instance(instance: Instance) -> Result<()> {
    call(|| unsafe { sys::nreDestroyInstance(instance.0) })
}

pub fn set_instance_transform(instance: Instance, transform: &Transform) -> Result<()> {
    let transform = sys::NreTransform::from(transform);
    call(|| unsafe { sys::nreSetInstanceTransform(instance.0, &transform) })
}

pub fn get_instance_transform(instance: Instance) -> Result<Transform> {
    get(|out| unsafe { sys::nreGetInstanceTransform(instance.0, out) }).map(Transform::from)
}

pub fn get_instance_matrix(instance: Instance) -> Result<Mat4> {
    get(|out| unsafe { sys::nreGetInstanceMatrix(instance.0, out) }).map(Mat4::from)
}

pub fn translate_instance(instance: Instance, offset: impl Into<Vec3>) -> Result<()> {
    let offset = offset.into().into();
    call(|| unsafe { sys::nreTranslateInstance(instance.0, offset) })
}

/// `angle` is in radians.
pub fn rotate_instance(instance: Instance, angle: f32, axis: impl Into<Vec3>) -> Result<()> {
    let axis = axis.into().into();
    call(|| unsafe { sys::nreRotateInstance(instance.0, angle, axis) })
}

pub fn scale_instance(instance: Instance, factor: impl Into<Vec3>) -> Result<()> {
    let factor = factor.into().into();
    call(|| unsafe { sys::nreScaleInstance(instance.0, factor) })
}

/// A hidden instance stays in its scenes but isn't drawn.
pub fn set_visibility(instance: Instance, visible: bool) -> Result<()> {
    call(|| unsafe { sys::nreSetVisibility(instance.0, visible) })
}

/// Whether the instance casts sun shadows (it still receives them). Default true; e.g. off for grass, for speed.
/// [`draw_model`] always casts.
pub fn set_casts_shadows(instance: Instance, casts_shadows: bool) -> Result<()> {
    call(|| unsafe { sys::nreSetCastsShadows(instance.0, casts_shadows) })
}

/// [`ApiMode::FunctionDraw`] only, between [`render_frame`] and [`end_frame`]: draws the model once with this
/// transform (`None`: the identity), no instance needed.
pub fn draw_model(model: Model, transform: Option<&Transform>) -> Result<()> {
    let transform = transform.map(sys::NreTransform::from);
    let ptr = transform.as_ref().map_or(std::ptr::null(), |t| t as *const sys::NreTransform);
    call(|| unsafe { sys::nreDrawModel(model.0, ptr) })
}

/// [`ApiMode::FunctionDraw`] only. Uses the instance's transform; a hidden instance isn't drawn.
pub fn draw_instance(instance: Instance) -> Result<()> {
    call(|| unsafe { sys::nreDrawInstance(instance.0) })
}

// ---------------------------------------------------------------------------
// Scenes
// ---------------------------------------------------------------------------

pub fn create_scene() -> Result<Scene> {
    get(|out| unsafe { sys::nreCreateScene(out) }).map(Scene)
}

pub fn destroy_scene(scene: Scene) -> Result<()> {
    call(|| unsafe { sys::nreDestroyScene(scene.0) })
}

pub fn bind_scene(scene: Scene) -> Result<()> {
    call(|| unsafe { sys::nreBindScene(scene.0) })
}

pub fn get_bound_scene() -> Result<Scene> {
    get(|out| unsafe { sys::nreGetBoundScene(out) }).map(Scene)
}

/// Adds to the bound scene. Adding an instance that's already in it does nothing.
pub fn add_instance_to_scene(instance: Instance) -> Result<()> {
    call(|| unsafe { sys::nreAddInstanceToScene(instance.0) })
}

pub fn remove_instance_from_scene(instance: Instance) -> Result<()> {
    call(|| unsafe { sys::nreRemoveInstanceFromScene(instance.0) })
}

// ---------------------------------------------------------------------------
// Textures and materials. Materials belong to the model: changing one changes every instance of that model.
// ---------------------------------------------------------------------------

/// [`create_texture_with`] with `srgb = true` (renderer.h's default): loads an image file (PNG, JPEG, ...) as a
/// color texture, for base colours.
pub fn create_texture(path: impl AsRef<Path>) -> Result<Texture> {
    create_texture_with(path, true)
}

/// Loads an image file (PNG, JPEG, ...). `srgb = true` for colour images (base colour), decoded from sRGB when
/// sampled; `false` for data maps that hold plain numbers (normal maps and metallic-roughness maps).
pub fn create_texture_with(path: impl AsRef<Path>, srgb: bool) -> Result<Texture> {
    let path = c_path(path.as_ref())?;
    get(|out| unsafe { sys::nreCreateTexture(path.as_ptr(), srgb, out) }).map(Texture)
}

/// Give the materials that use this texture another one first: they keep pointing at it.
pub fn destroy_texture(texture: Texture) -> Result<()> {
    call(|| unsafe { sys::nreDestroyTexture(texture.0) })
}

/// `None` as base texture uses the default texture. The metallic and normal textures are stored but not used yet.
pub fn create_material(
    base_texture: Option<Texture>,
    metallic_texture: Option<Texture>,
    normal_texture: Option<Texture>,
) -> Result<Material> {
    let raw = |t: Option<Texture>| t.unwrap_or(Texture::NONE).0;
    let (base, metallic, normal) = (raw(base_texture), raw(metallic_texture), raw(normal_texture));
    get(|out| unsafe { sys::nreCreateMaterial(base, metallic, normal, out) }).map(Material)
}

/// Models still using it fall back to the default material.
pub fn destroy_material(material: Material) -> Result<()> {
    call(|| unsafe { sys::nreDestroyMaterial(material.0) })
}

/// `None` uses the default texture. Waits for the GPU to finish the frames in flight: don't call it every frame.
pub fn set_material_base_texture(material: Material, texture: Option<Texture>) -> Result<()> {
    let texture = texture.unwrap_or(Texture::NONE).0;
    call(|| unsafe { sys::nreSetMaterialBaseTexture(material.0, texture) })
}

/// The model's material slots, in the file's material order. Empty for a model without materials (like OBJ).
pub fn get_model_materials(model: Model) -> Result<Vec<Material>> {
    let mut count: u32 = get(|count| unsafe { sys::nreGetModelMaterials(model.0, count, std::ptr::null_mut()) })?;
    let mut materials = vec![Material::NONE; count as usize];
    // Material is repr(transparent) over NreMaterial, so the Vec's buffer is an NreMaterial array.
    let ptr = materials.as_mut_ptr().cast::<sys::NreMaterial>();
    call(|| unsafe { sys::nreGetModelMaterials(model.0, &mut count, ptr) })?;
    materials.truncate(count as usize);
    Ok(materials)
}

/// Makes every mesh of the model use `material`, replacing all of its slots.
pub fn set_model_material(model: Model, material: Material) -> Result<()> {
    call(|| unsafe { sys::nreSetModelMaterial(model.0, material.0) })
}

/// Replaces slot `index` (an index into [`get_model_materials`]), so only the meshes using that slot change.
pub fn replace_model_material(model: Model, index: u32, material: Material) -> Result<()> {
    call(|| unsafe { sys::nreReplaceModelMaterial(model.0, index, material.0) })
}

// ---------------------------------------------------------------------------
// Lights and the sun. Create a light, then add it to the bound scene. `direction` is the way the light travels.
// ---------------------------------------------------------------------------

/// [`create_directional_light_with`] with white light of intensity 3 (renderer.h's defaults).
pub fn create_directional_light(direction: impl Into<Vec3>) -> Result<Light> {
    create_directional_light_with(direction, Vec3::ONE, 3.0)
}

pub fn create_directional_light_with(
    direction: impl Into<Vec3>,
    color: impl Into<Vec3>,
    intensity: f32,
) -> Result<Light> {
    let (direction, color) = (direction.into().into(), color.into().into());
    get(|out| unsafe { sys::nreCreateDirectionalLight(direction, color, intensity, out) }).map(Light)
}

/// [`create_point_light_with`] with white light of intensity 10 (renderer.h's defaults). Falls off with 1/distance².
pub fn create_point_light(position: impl Into<Vec3>) -> Result<Light> {
    create_point_light_with(position, Vec3::ONE, 10.0)
}

pub fn create_point_light_with(position: impl Into<Vec3>, color: impl Into<Vec3>, intensity: f32) -> Result<Light> {
    let (position, color) = (position.into().into(), color.into().into());
    get(|out| unsafe { sys::nreCreatePointLight(position, color, intensity, out) }).map(Light)
}

/// Also removes it from every scene it's in.
pub fn destroy_light(light: Light) -> Result<()> {
    call(|| unsafe { sys::nreDestroyLight(light.0) })
}

/// Adds to the bound scene. Adding a light that's already in it does nothing.
pub fn add_light_to_scene(light: Light) -> Result<()> {
    call(|| unsafe { sys::nreAddLightToScene(light.0) })
}

pub fn remove_light_from_scene(light: Light) -> Result<()> {
    call(|| unsafe { sys::nreRemoveLightFromScene(light.0) })
}

/// Only used by point lights.
pub fn set_light_position(light: Light, position: impl Into<Vec3>) -> Result<()> {
    let position = position.into().into();
    call(|| unsafe { sys::nreSetLightPosition(light.0, position) })
}

/// Only used by directional lights.
pub fn set_light_direction(light: Light, direction: impl Into<Vec3>) -> Result<()> {
    let direction = direction.into().into();
    call(|| unsafe { sys::nreSetLightDirection(light.0, direction) })
}

pub fn set_light_color(light: Light, color: impl Into<Vec3>, intensity: f32) -> Result<()> {
    let color = color.into().into();
    call(|| unsafe { sys::nreSetLightColor(light.0, color, intensity) })
}

/// [`set_sun_with`] with white light of intensity 3 (renderer.h's defaults).
pub fn set_sun(direction: impl Into<Vec3>) -> Result<()> {
    set_sun_with(direction, Vec3::ONE, 3.0)
}

/// The bound scene's sun: one directional light kept apart from the other lights (see renderer.h for how
/// shaders read it). A scene starts without one (intensity 0).
pub fn set_sun_with(direction: impl Into<Vec3>, color: impl Into<Vec3>, intensity: f32) -> Result<()> {
    let (direction, color) = (direction.into().into(), color.into().into());
    call(|| unsafe { sys::nreSetSun(direction, color, intensity) })
}

/// Keeps the color and intensity, e.g. for a day/night cycle.
pub fn set_sun_direction(direction: impl Into<Vec3>) -> Result<()> {
    let direction = direction.into().into();
    call(|| unsafe { sys::nreSetSunDirection(direction) })
}

pub fn get_sun_direction() -> Result<Vec3> {
    get(|out| unsafe { sys::nreGetSunDirection(out) }).map(Vec3::from)
}

// ---------------------------------------------------------------------------
// Scene lighting, shadows, haze, exposure and sky colour, on the bound scene. renderer.h's defaults:
// Small: shadows 20 m, haze off. Landscape: shadows 500 m, haze 8000 m / 300 m. Exposure 1, sky (0.1, 0.3, 0.6).
// ---------------------------------------------------------------------------

/// Switches the bound scene's lighting mode, and resets shadow distance, haze and the sky to that mode's
/// defaults. The sun, lights, exposure and sky colour are kept. Landscape's sky comes from the sun: without one
/// the scene shows night.
pub fn set_scene_lighting(lighting: SceneLighting) -> Result<()> {
    call(|| unsafe { sys::nreSetSceneLighting(lighting.to_raw()) })
}

/// The bound scene's lighting mode. A value these bindings don't know fails with [`ErrorCode::Unknown`].
pub fn get_scene_lighting() -> Result<SceneLighting> {
    let raw = get(|out| unsafe { sys::nreGetSceneLighting(out) })?;
    SceneLighting::from_raw(raw)
        .ok_or_else(|| Error::new(ErrorCode::Unknown, format!("libnre_c returned an unknown SceneLighting: {raw}")))
}

/// How far from the camera the sun casts shadows, in metres. 0 turns sun shadows off; negative fails with
/// [`ErrorCode::InvalidArgument`]. Shorter distances give sharper shadows.
pub fn set_shadow_distance(meters: f32) -> Result<()> {
    call(|| unsafe { sys::nreSetShadowDistance(meters) })
}

/// [`set_haze_with`] with a 300 m thick haze layer (renderer.h's default).
pub fn set_haze(visibility: f32) -> Result<()> {
    set_haze_with(visibility, 300.0)
}

/// Distance haze: far things fade into the sky's colour. `visibility`: metres at which the haze hides almost
/// everything, 0 turns it off, not negative. `height`: the haze layer's thickness in metres above y = 0, greater
/// than 0.
pub fn set_haze_with(visibility: f32, height: f32) -> Result<()> {
    call(|| unsafe { sys::nreSetHaze(visibility, height) })
}

/// The bound scene's exposure: 2 is twice as bright, 0.5 half. Greater than 0. In Landscape with the procedural
/// sky it multiplies the automatic exposure.
pub fn set_exposure(exposure: f32) -> Result<()> {
    call(|| unsafe { sys::nreSetExposure(exposure) })
}

/// The background where nothing is drawn and there's no skybox (Small), linear 0..1 as shown on screen.
pub fn set_sky_color(color: impl Into<Vec3>) -> Result<()> {
    let color = color.into().into();
    call(|| unsafe { sys::nreSetSkyColor(color) })
}

// ---------------------------------------------------------------------------
// Cubemaps and the skybox
// ---------------------------------------------------------------------------

/// Six square images of the same size, loaded as color (sRGB), in the OpenGL order: right (+X), left (-X),
/// top (+Y), bottom (-Y), front (+Z), back (-Z).
pub fn create_cubemap(
    right: impl AsRef<Path>,
    left: impl AsRef<Path>,
    top: impl AsRef<Path>,
    bottom: impl AsRef<Path>,
    front: impl AsRef<Path>,
    back: impl AsRef<Path>,
) -> Result<Cubemap> {
    let [right, left, top, bottom, front, back] = [
        c_path(right.as_ref())?,
        c_path(left.as_ref())?,
        c_path(top.as_ref())?,
        c_path(bottom.as_ref())?,
        c_path(front.as_ref())?,
        c_path(back.as_ref())?,
    ];
    get(|out| unsafe {
        sys::nreCreateCubemap(
            right.as_ptr(),
            left.as_ptr(),
            top.as_ptr(),
            bottom.as_ptr(),
            front.as_ptr(),
            back.as_ptr(),
            out,
        )
    })
    .map(Cubemap)
}

/// Waits for the GPU to finish the frames in flight. Scenes using it go back to having no skybox.
pub fn destroy_cubemap(cubemap: Cubemap) -> Result<()> {
    call(|| unsafe { sys::nreDestroyCubemap(cubemap.0) })
}

/// The bound scene's sky, drawn behind everything (3D only). `None` removes it.
pub fn set_skybox(cubemap: Option<Cubemap>) -> Result<()> {
    let cubemap = cubemap.unwrap_or(Cubemap::NONE).0;
    call(|| unsafe { sys::nreSetSkybox(cubemap) })
}

// ---------------------------------------------------------------------------
// Camera. Angles are in radians.
// ---------------------------------------------------------------------------

pub fn set_camera_position(position: impl Into<Vec3>) -> Result<()> {
    let position = position.into().into();
    call(|| unsafe { sys::nreSetCameraPosition(position) })
}

pub fn get_camera_position() -> Result<Vec3> {
    get(|out| unsafe { sys::nreGetCameraPosition(out) }).map(Vec3::from)
}

pub fn move_camera(offset: impl Into<Vec3>) -> Result<()> {
    let offset = offset.into().into();
    call(|| unsafe { sys::nreMoveCamera(offset) })
}

pub fn move_camera_relative(offset: impl Into<Vec3>) -> Result<()> {
    let offset = offset.into().into();
    call(|| unsafe { sys::nreMoveCameraRelative(offset) })
}

pub fn set_camera_rotation(yaw: f32, pitch: f32) -> Result<()> {
    call(|| unsafe { sys::nreSetCameraRotation(yaw, pitch) })
}

pub fn rotate_camera(yaw_delta: f32, pitch_delta: f32) -> Result<()> {
    call(|| unsafe { sys::nreRotateCamera(yaw_delta, pitch_delta) })
}

pub fn camera_look_at(target: impl Into<Vec3>) -> Result<()> {
    let target = target.into().into();
    call(|| unsafe { sys::nreCameraLookAt(target) })
}

pub fn get_camera_forward() -> Result<Vec3> {
    get(|out| unsafe { sys::nreGetCameraForward(out) }).map(Vec3::from)
}

pub fn get_view_matrix() -> Result<Mat4> {
    get(|out| unsafe { sys::nreGetViewMatrix(out) }).map(Mat4::from)
}

/// The projection the scene is drawn with. Vulkan conventions, not OpenGL's: clip space has +Y down (the flip is
/// in the matrix) and depth is reverse-Z with an infinite far plane. In Render_2D, the 2D scene's orthographic one.
pub fn get_projection_matrix() -> Result<Mat4> {
    get(|out| unsafe { sys::nreGetProjectionMatrix(out) }).map(Mat4::from)
}

/// The nearest visible instance of the bound scene under a pixel, or `None`. `window_pos` is in
/// [`get_mouse_position`]'s units (window pixels, top-left origin). Render_3D only (Render_2D:
/// [`ErrorCode::InvalidArgument`] in Debug builds); fails with [`ErrorCode::NoSceneBound`] without a bound scene.
pub fn pick_instance(window_pos: impl Into<Vec2>) -> Result<Option<RayHit>> {
    let window_pos = window_pos.into().into();
    let mut hit = false;
    let ray_hit = get(|out| unsafe { sys::nrePickInstance(window_pos, &mut hit, out) })?;
    Ok(hit.then(|| RayHit::from(ray_hit)))
}

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------

/// Frames per second, averaged over the last half second.
pub fn get_fps() -> Result<f32> {
    get(|out| unsafe { sys::nreGetFPS(out) })
}

/// Seconds between the last two presented frames. Multiply movement by this to make it frame-rate independent.
pub fn get_delta_time() -> Result<f32> {
    get(|out| unsafe { sys::nreGetDeltaTime(out) })
}

// ---------------------------------------------------------------------------
// Input. The deltas cover the time between the last two poll_events calls. "Pressed" / "released" mean the
// button or key went down / up during the last poll_events call, so they're true for one frame.
// ---------------------------------------------------------------------------

pub fn is_mouse_button_down(button: MouseButton) -> Result<bool> {
    get(|out| unsafe { sys::nreIsMouseButtonDown(button.to_raw(), out) })
}

pub fn is_mouse_button_pressed(button: MouseButton) -> Result<bool> {
    get(|out| unsafe { sys::nreIsMouseButtonPressed(button.to_raw(), out) })
}

pub fn is_mouse_button_released(button: MouseButton) -> Result<bool> {
    get(|out| unsafe { sys::nreIsMouseButtonReleased(button.to_raw(), out) })
}

/// How far the cursor moved, in pixels. +x is right, +y is down.
pub fn get_mouse_delta() -> Result<Vec2> {
    get(|out| unsafe { sys::nreGetMouseDelta(out) }).map(Vec2::from)
}

/// Scroll wheel steps. Positive means scrolling up / away from you.
pub fn get_scroll_delta() -> Result<f32> {
    get(|out| unsafe { sys::nreGetScrollDelta(out) })
}

/// Cursor position in pixels from the window's top-left corner, as of the last poll_events call.
pub fn get_mouse_position() -> Result<Vec2> {
    get(|out| unsafe { sys::nreGetMousePosition(out) }).map(Vec2::from)
}

pub fn set_cursor_mode(mode: CursorMode) -> Result<()> {
    call(|| unsafe { sys::nreSetCursorMode(mode.to_raw()) })
}

pub fn is_key_down(key: Key) -> Result<bool> {
    get(|out| unsafe { sys::nreIsKeyDown(key.to_raw(), out) })
}

/// Holding a key doesn't repeat it.
pub fn is_key_pressed(key: Key) -> Result<bool> {
    get(|out| unsafe { sys::nreIsKeyPressed(key.to_raw(), out) })
}

pub fn is_key_released(key: Key) -> Result<bool> {
    get(|out| unsafe { sys::nreIsKeyReleased(key.to_raw(), out) })
}
