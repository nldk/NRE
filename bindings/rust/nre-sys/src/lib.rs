//! Raw FFI declarations for libnre_c, the C API of the NRE renderer: a hand-written mirror of `include/nre.h`.
//!
//! Everything here is `unsafe` and exactly as in the header; see nre.h for the rules (pointers must not be
//! NULL unless noted, strings are NUL-terminated UTF-8, `{0}` is the empty handle) and renderer.h for what each
//! function does. Use the `nre` crate for the safe API.
//!
//! C enums are `i32` type aliases plus constants, so any value the library returns can be read without UB.
//! Linking: the build script links `nre_c` as a dylib, searching `NRE_LIB_DIR` first when it's set.

#![allow(non_camel_case_types, non_snake_case)]

use std::ffi::c_char;

// ---------------------------------------------------------------------------
// Enums. The values never change; new ones are added at the end.
// ---------------------------------------------------------------------------
pub type NreErrorCode = i32;
pub const NRE_OK: NreErrorCode = 0;
pub const NRE_ERROR_UNSUPPORTED_GPU: NreErrorCode = 1;
pub const NRE_ERROR_CREATING_VULKAN_INSTANCE: NreErrorCode = 2;
pub const NRE_ERROR_CREATING_SWAPCHAIN: NreErrorCode = 3;
pub const NRE_ERROR_RECREATING_SWAPCHAIN: NreErrorCode = 4;
pub const NRE_ERROR_CREATING_PIPELINE: NreErrorCode = 5;
pub const NRE_ERROR_WINDOW_CREATION_FAILED: NreErrorCode = 6;
pub const NRE_ERROR_FILE_NOT_FOUND: NreErrorCode = 7;
pub const NRE_ERROR_WRONG_MODE: NreErrorCode = 8;
pub const NRE_ERROR_NOT_INITIALIZED: NreErrorCode = 9;
pub const NRE_ERROR_ALREADY_INITIALIZED: NreErrorCode = 10;
pub const NRE_ERROR_INVALID_HANDLE: NreErrorCode = 11;
pub const NRE_ERROR_UNSUPPORTED_FORMAT: NreErrorCode = 12;
pub const NRE_ERROR_MODEL_LOAD_FAILED: NreErrorCode = 13;
pub const NRE_ERROR_FAILED_TO_CREATE_ALLOCATOR: NreErrorCode = 14;
pub const NRE_ERROR_BUFFER_CREATION_FAILED: NreErrorCode = 15;
pub const NRE_ERROR_UPLOAD_FAILED: NreErrorCode = 16;
pub const NRE_ERROR_NO_PIPELINE_BOUND: NreErrorCode = 17;
pub const NRE_ERROR_NO_SCENE_BOUND: NreErrorCode = 18;
pub const NRE_ERROR_CREATING_TEXTURE: NreErrorCode = 19;
pub const NRE_ERROR_IMAGE_CREATION_FAILED: NreErrorCode = 20;
pub const NRE_ERROR_IMAGE_VIEW_CREATION_FAILED: NreErrorCode = 21;
pub const NRE_ERROR_INDEX_OUT_OF_RANGE: NreErrorCode = 22;
pub const NRE_ERROR_IMPOSSIBLE_TYPE_FOR_PER_FRAME_BUFFER: NreErrorCode = 23;
pub const NRE_ERROR_BUFFER_WITH_SIZE_NULL: NreErrorCode = 24;
pub const NRE_ERROR_SIZE_OF_BUFFER_TOO_BIG: NreErrorCode = 25;
pub const NRE_ERROR_DESCRIPTOR_CREATION_FAILED: NreErrorCode = 26;
pub const NRE_ERROR_SHADER_CREATION_FAILED: NreErrorCode = 27;
pub const NRE_ERROR_UNKNOWN: NreErrorCode = 28;
pub const NRE_ERROR_LIBRARY_LOAD_FAILED: NreErrorCode = 29;
/// E.g. a negative shadow distance, or picking in Render_2D.
pub const NRE_ERROR_INVALID_ARGUMENT: NreErrorCode = 30;
pub const NRE_ERROR_CODE_MAX_ENUM: NreErrorCode = 0x7FFF_FFFF;

pub type NreShaderType = i32;
pub const NRE_SHADER_FRAGMENT: NreShaderType = 0;
pub const NRE_SHADER_VERTEX: NreShaderType = 1;
pub const NRE_SHADER_GEOMETRY: NreShaderType = 2;
pub const NRE_SHADER_COMPUTE: NreShaderType = 3;
pub const NRE_SHADER_TYPE_MAX_ENUM: NreShaderType = 0x7FFF_FFFF;

pub type NreRenderMode = i32;
pub const NRE_RENDER_3D: NreRenderMode = 0;
pub const NRE_RENDER_2D: NreRenderMode = 1;
pub const NRE_RENDER_MODE_MAX_ENUM: NreRenderMode = 0x7FFF_FFFF;

pub type NreApiMode = i32;
pub const NRE_API_MODE_SCENE: NreApiMode = 0;
pub const NRE_API_MODE_FUNCTION_DRAW: NreApiMode = 1;
pub const NRE_API_MODE_MAX_ENUM: NreApiMode = 0x7FFF_FFFF;

/// A scene's lighting mode (`nreSetSceneLighting`); see renderer.h's `SceneLighting`.
pub type NreSceneLighting = i32;
/// Default: cubemap or sky colour, short sharp shadows, no haze.
pub const NRE_SCENE_LIGHTING_SMALL: NreSceneLighting = 0;
/// Procedural sky from the sun, far cascaded shadows, haze, auto exposure.
pub const NRE_SCENE_LIGHTING_LANDSCAPE: NreSceneLighting = 1;
pub const NRE_SCENE_LIGHTING_MAX_ENUM: NreSceneLighting = 0x7FFF_FFFF;

pub type NreRenderErrorPolicy = i32;
pub const NRE_ERROR_POLICY_IGNORE: NreRenderErrorPolicy = 0;
pub const NRE_ERROR_POLICY_RETURN: NreRenderErrorPolicy = 1;
pub const NRE_ERROR_POLICY_PANIC: NreRenderErrorPolicy = 2;
pub const NRE_ERROR_POLICY_MAX_ENUM: NreRenderErrorPolicy = 0x7FFF_FFFF;

pub type NreMouseButton = i32;
pub const NRE_MOUSE_BUTTON_LEFT: NreMouseButton = 0;
pub const NRE_MOUSE_BUTTON_RIGHT: NreMouseButton = 1;
pub const NRE_MOUSE_BUTTON_MIDDLE: NreMouseButton = 2;
pub const NRE_MOUSE_BUTTON_MAX_ENUM: NreMouseButton = 0x7FFF_FFFF;

pub type NreCursorMode = i32;
pub const NRE_CURSOR_NORMAL: NreCursorMode = 0;
pub const NRE_CURSOR_HIDDEN: NreCursorMode = 1;
pub const NRE_CURSOR_LOCKED: NreCursorMode = 2;
pub const NRE_CURSOR_MODE_MAX_ENUM: NreCursorMode = 0x7FFF_FFFF;

pub type NreKey = i32;
pub const NRE_KEY_SPACE: NreKey = 32;
pub const NRE_KEY_APOSTROPHE: NreKey = 39;
pub const NRE_KEY_COMMA: NreKey = 44;
pub const NRE_KEY_MINUS: NreKey = 45;
pub const NRE_KEY_PERIOD: NreKey = 46;
pub const NRE_KEY_SLASH: NreKey = 47;
pub const NRE_KEY_0: NreKey = 48;
pub const NRE_KEY_1: NreKey = 49;
pub const NRE_KEY_2: NreKey = 50;
pub const NRE_KEY_3: NreKey = 51;
pub const NRE_KEY_4: NreKey = 52;
pub const NRE_KEY_5: NreKey = 53;
pub const NRE_KEY_6: NreKey = 54;
pub const NRE_KEY_7: NreKey = 55;
pub const NRE_KEY_8: NreKey = 56;
pub const NRE_KEY_9: NreKey = 57;
pub const NRE_KEY_SEMICOLON: NreKey = 59;
pub const NRE_KEY_EQUAL: NreKey = 61;
pub const NRE_KEY_A: NreKey = 65;
pub const NRE_KEY_B: NreKey = 66;
pub const NRE_KEY_C: NreKey = 67;
pub const NRE_KEY_D: NreKey = 68;
pub const NRE_KEY_E: NreKey = 69;
pub const NRE_KEY_F: NreKey = 70;
pub const NRE_KEY_G: NreKey = 71;
pub const NRE_KEY_H: NreKey = 72;
pub const NRE_KEY_I: NreKey = 73;
pub const NRE_KEY_J: NreKey = 74;
pub const NRE_KEY_K: NreKey = 75;
pub const NRE_KEY_L: NreKey = 76;
pub const NRE_KEY_M: NreKey = 77;
pub const NRE_KEY_N: NreKey = 78;
pub const NRE_KEY_O: NreKey = 79;
pub const NRE_KEY_P: NreKey = 80;
pub const NRE_KEY_Q: NreKey = 81;
pub const NRE_KEY_R: NreKey = 82;
pub const NRE_KEY_S: NreKey = 83;
pub const NRE_KEY_T: NreKey = 84;
pub const NRE_KEY_U: NreKey = 85;
pub const NRE_KEY_V: NreKey = 86;
pub const NRE_KEY_W: NreKey = 87;
pub const NRE_KEY_X: NreKey = 88;
pub const NRE_KEY_Y: NreKey = 89;
pub const NRE_KEY_Z: NreKey = 90;
pub const NRE_KEY_LEFT_BRACKET: NreKey = 91;
pub const NRE_KEY_BACKSLASH: NreKey = 92;
pub const NRE_KEY_RIGHT_BRACKET: NreKey = 93;
pub const NRE_KEY_GRAVE_ACCENT: NreKey = 96;
pub const NRE_KEY_ESCAPE: NreKey = 256;
pub const NRE_KEY_ENTER: NreKey = 257;
pub const NRE_KEY_TAB: NreKey = 258;
pub const NRE_KEY_BACKSPACE: NreKey = 259;
pub const NRE_KEY_INSERT: NreKey = 260;
pub const NRE_KEY_DELETE: NreKey = 261;
pub const NRE_KEY_RIGHT: NreKey = 262;
pub const NRE_KEY_LEFT: NreKey = 263;
pub const NRE_KEY_DOWN: NreKey = 264;
pub const NRE_KEY_UP: NreKey = 265;
pub const NRE_KEY_PAGE_UP: NreKey = 266;
pub const NRE_KEY_PAGE_DOWN: NreKey = 267;
pub const NRE_KEY_HOME: NreKey = 268;
pub const NRE_KEY_END: NreKey = 269;
pub const NRE_KEY_CAPS_LOCK: NreKey = 280;
pub const NRE_KEY_SCROLL_LOCK: NreKey = 281;
pub const NRE_KEY_NUM_LOCK: NreKey = 282;
pub const NRE_KEY_PRINT_SCREEN: NreKey = 283;
pub const NRE_KEY_PAUSE: NreKey = 284;
pub const NRE_KEY_F1: NreKey = 290;
pub const NRE_KEY_F2: NreKey = 291;
pub const NRE_KEY_F3: NreKey = 292;
pub const NRE_KEY_F4: NreKey = 293;
pub const NRE_KEY_F5: NreKey = 294;
pub const NRE_KEY_F6: NreKey = 295;
pub const NRE_KEY_F7: NreKey = 296;
pub const NRE_KEY_F8: NreKey = 297;
pub const NRE_KEY_F9: NreKey = 298;
pub const NRE_KEY_F10: NreKey = 299;
pub const NRE_KEY_F11: NreKey = 300;
pub const NRE_KEY_F12: NreKey = 301;
pub const NRE_KEY_KP_0: NreKey = 320;
pub const NRE_KEY_KP_1: NreKey = 321;
pub const NRE_KEY_KP_2: NreKey = 322;
pub const NRE_KEY_KP_3: NreKey = 323;
pub const NRE_KEY_KP_4: NreKey = 324;
pub const NRE_KEY_KP_5: NreKey = 325;
pub const NRE_KEY_KP_6: NreKey = 326;
pub const NRE_KEY_KP_7: NreKey = 327;
pub const NRE_KEY_KP_8: NreKey = 328;
pub const NRE_KEY_KP_9: NreKey = 329;
pub const NRE_KEY_KP_DECIMAL: NreKey = 330;
pub const NRE_KEY_KP_DIVIDE: NreKey = 331;
pub const NRE_KEY_KP_MULTIPLY: NreKey = 332;
pub const NRE_KEY_KP_SUBTRACT: NreKey = 333;
pub const NRE_KEY_KP_ADD: NreKey = 334;
pub const NRE_KEY_KP_ENTER: NreKey = 335;
pub const NRE_KEY_KP_EQUAL: NreKey = 336;
pub const NRE_KEY_LEFT_SHIFT: NreKey = 340;
pub const NRE_KEY_LEFT_CONTROL: NreKey = 341;
pub const NRE_KEY_LEFT_ALT: NreKey = 342;
pub const NRE_KEY_LEFT_SUPER: NreKey = 343;
pub const NRE_KEY_RIGHT_SHIFT: NreKey = 344;
pub const NRE_KEY_RIGHT_CONTROL: NreKey = 345;
pub const NRE_KEY_RIGHT_ALT: NreKey = 346;
pub const NRE_KEY_RIGHT_SUPER: NreKey = 347;
pub const NRE_KEY_MENU: NreKey = 348;
pub const NRE_KEY_MAX_ENUM: NreKey = 0x7FFF_FFFF;

// ---------------------------------------------------------------------------
// Math types
// ---------------------------------------------------------------------------
#[repr(C)]
#[derive(Copy, Clone, Debug, Default, PartialEq)]
pub struct NreVec2 {
    pub x: f32,
    pub y: f32,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default, PartialEq)]
pub struct NreVec3 {
    pub x: f32,
    pub y: f32,
    pub z: f32,
}

/// Identity: `{0, 0, 0, 1}`.
#[repr(C)]
#[derive(Copy, Clone, Debug, Default, PartialEq)]
pub struct NreQuat {
    pub x: f32,
    pub y: f32,
    pub z: f32,
    pub w: f32,
}

/// Column-major: `m[column * 4 + row]`.
#[repr(C)]
#[derive(Copy, Clone, Debug, Default, PartialEq)]
pub struct NreMat4 {
    pub m: [f32; 16],
}

/// One vertex for `nreCreateMeshModel`. A normal of `{0,0,0}` is computed from the triangles using the vertex.
#[repr(C)]
#[derive(Copy, Clone, Debug, Default, PartialEq)]
pub struct NreVertex {
    pub position: NreVec3,
    pub normal: NreVec3,
    pub uv: NreVec2,
}

/// Applied as scale, then rotation, then translation.
#[repr(C)]
#[derive(Copy, Clone, Debug, Default, PartialEq)]
pub struct NreTransform {
    pub position: NreVec3,
    pub rotation: NreQuat,
    pub scale: NreVec3,
}

// ---------------------------------------------------------------------------
// Handles. `{0}` (Default) is the empty handle.
// ---------------------------------------------------------------------------
macro_rules! handles {
    ($($name:ident)*) => {$(
        #[repr(C)]
        #[derive(Copy, Clone, Debug, Default, PartialEq, Eq, Hash)]
        pub struct $name {
            pub index: u32,
            pub generation: u32,
        }
    )*};
}
handles!(NreShader NrePipeline NreModel NreInstance NreLight NreScene NreTexture NreMaterial NreCubemap);

/// What `nrePickInstance` hit: the instance, the distance from the camera along the ray (world units) and the
/// world-space point.
#[repr(C)]
#[derive(Copy, Clone, Debug, Default, PartialEq)]
pub struct NreRayHit {
    pub instance: NreInstance,
    pub distance: f32,
    pub point: NreVec3,
}

// The layouts nre.h promises.
const _: () = {
    use std::mem::{align_of, offset_of, size_of};
    assert!(size_of::<NreVec2>() == 8 && size_of::<NreVec3>() == 12 && size_of::<NreQuat>() == 16);
    assert!(size_of::<NreMat4>() == 64 && size_of::<NreTransform>() == 40 && align_of::<NreTransform>() == 4);
    assert!(offset_of!(NreTransform, rotation) == 12 && offset_of!(NreTransform, scale) == 28);
    assert!(size_of::<NreModel>() == 8 && size_of::<NreCubemap>() == 8);
    assert!(size_of::<NreVertex>() == 32 && offset_of!(NreVertex, normal) == 12 && offset_of!(NreVertex, uv) == 24);
    assert!(size_of::<NreRayHit>() == 24 && offset_of!(NreRayHit, distance) == 8 && offset_of!(NreRayHit, point) == 12);
};

unsafe extern "C" {
    // Errors and version
    pub fn nreGetLastErrorMessage() -> *const c_char;
    pub fn nreErrorCodeToString(code: NreErrorCode) -> *const c_char;
    pub fn nreGetVersion(major: *mut u32, minor: *mut u32, patch: *mut u32);

    // Setup and frames
    pub fn nreSetRenderConfig(mode: NreRenderMode, apiMode: NreApiMode, errorPolicy: NreRenderErrorPolicy,
                              manualFramePresentationAndSubmission: bool, windowWidth: i32, windowHeight: i32,
                              windowTitle: *const c_char, resizable: bool, showFpsInTitle: bool) -> NreErrorCode;
    pub fn nreInit() -> NreErrorCode;
    pub fn nreShutdown() -> NreErrorCode;
    pub fn nreRenderFrame() -> NreErrorCode;
    pub fn nreEndFrame() -> NreErrorCode;
    pub fn nrePresentFrame() -> NreErrorCode;
    pub fn nreSubmitFrame() -> NreErrorCode;
    pub fn nreWindowShouldClose(shouldClose: *mut bool) -> NreErrorCode;
    pub fn nrePollEvents() -> NreErrorCode;

    // Shaders and pipelines
    pub fn nreCreateShader(shaderPath: *const c_char, r#type: NreShaderType, shader: *mut NreShader) -> NreErrorCode;
    pub fn nreDestroyShader(shader: NreShader) -> NreErrorCode;
    pub fn nreCreatePipeline(vertexShader: NreShader, fragmentShader: NreShader, pipeline: *mut NrePipeline)
                             -> NreErrorCode;
    pub fn nreDestroyPipeline(pipeline: NrePipeline) -> NreErrorCode;
    pub fn nreBindPipeline(pipeline: NrePipeline) -> NreErrorCode;

    // Models and instances
    pub fn nreCreateModel(modelPath: *const c_char, model: *mut NreModel) -> NreErrorCode;
    /// A model from your own triangles (counter-clockwise front faces). The arrays are copied; material `{0}` =
    /// default.
    pub fn nreCreateMeshModel(vertices: *const NreVertex, vertexCount: u32, indices: *const u32, indexCount: u32,
                              material: NreMaterial, model: *mut NreModel) -> NreErrorCode;
    pub fn nreDestroyModel(model: NreModel) -> NreErrorCode;
    pub fn nreCreateInstance(model: NreModel, instance: *mut NreInstance) -> NreErrorCode;
    pub fn nreDestroyInstance(instance: NreInstance) -> NreErrorCode;
    pub fn nreSetInstanceTransform(instance: NreInstance, transform: *const NreTransform) -> NreErrorCode;
    pub fn nreGetInstanceTransform(instance: NreInstance, transform: *mut NreTransform) -> NreErrorCode;
    pub fn nreGetInstanceMatrix(instance: NreInstance, matrix: *mut NreMat4) -> NreErrorCode;
    pub fn nreTranslateInstance(instance: NreInstance, offset: NreVec3) -> NreErrorCode;
    pub fn nreRotateInstance(instance: NreInstance, angle: f32, axis: NreVec3) -> NreErrorCode;
    pub fn nreScaleInstance(instance: NreInstance, factor: NreVec3) -> NreErrorCode;
    pub fn nreSetVisibility(instance: NreInstance, visible: bool) -> NreErrorCode;
    /// Whether the instance casts sun shadows (it still receives them). Default true.
    pub fn nreSetCastsShadows(instance: NreInstance, castsShadows: bool) -> NreErrorCode;
    /// `transform` may be NULL: the identity transform.
    pub fn nreDrawModel(model: NreModel, transform: *const NreTransform) -> NreErrorCode;
    pub fn nreDrawInstance(instance: NreInstance) -> NreErrorCode;

    // Scenes
    pub fn nreCreateScene(scene: *mut NreScene) -> NreErrorCode;
    pub fn nreDestroyScene(scene: NreScene) -> NreErrorCode;
    pub fn nreBindScene(scene: NreScene) -> NreErrorCode;
    pub fn nreGetBoundScene(scene: *mut NreScene) -> NreErrorCode;
    pub fn nreAddInstanceToScene(instance: NreInstance) -> NreErrorCode;
    pub fn nreRemoveInstanceFromScene(instance: NreInstance) -> NreErrorCode;

    // Textures and materials. Empty texture handles use the default texture.
    /// `srgb`: true for colour images (base colour); false for data maps (normal and metallic-roughness maps).
    pub fn nreCreateTexture(path: *const c_char, srgb: bool, texture: *mut NreTexture) -> NreErrorCode;
    pub fn nreDestroyTexture(texture: NreTexture) -> NreErrorCode;
    pub fn nreCreateMaterial(baseTexture: NreTexture, metallicTexture: NreTexture, normalTexture: NreTexture,
                             material: *mut NreMaterial) -> NreErrorCode;
    pub fn nreDestroyMaterial(material: NreMaterial) -> NreErrorCode;
    pub fn nreSetMaterialBaseTexture(material: NreMaterial, texture: NreTexture) -> NreErrorCode;
    /// Two-call enumerate: with `materials` NULL, sets `*count` to the number of slots; otherwise `*count` is the
    /// array's size, and it's set to how many were written.
    pub fn nreGetModelMaterials(model: NreModel, count: *mut u32, materials: *mut NreMaterial) -> NreErrorCode;
    pub fn nreSetModelMaterial(model: NreModel, material: NreMaterial) -> NreErrorCode;
    pub fn nreReplaceModelMaterial(model: NreModel, index: u32, material: NreMaterial) -> NreErrorCode;

    // Lights and the sun
    pub fn nreCreateDirectionalLight(direction: NreVec3, color: NreVec3, intensity: f32, light: *mut NreLight)
                                     -> NreErrorCode;
    pub fn nreCreatePointLight(position: NreVec3, color: NreVec3, intensity: f32, light: *mut NreLight)
                               -> NreErrorCode;
    pub fn nreDestroyLight(light: NreLight) -> NreErrorCode;
    pub fn nreAddLightToScene(light: NreLight) -> NreErrorCode;
    pub fn nreRemoveLightFromScene(light: NreLight) -> NreErrorCode;
    pub fn nreSetLightPosition(light: NreLight, position: NreVec3) -> NreErrorCode;
    pub fn nreSetLightDirection(light: NreLight, direction: NreVec3) -> NreErrorCode;
    pub fn nreSetLightColor(light: NreLight, color: NreVec3, intensity: f32) -> NreErrorCode;
    pub fn nreSetSun(direction: NreVec3, color: NreVec3, intensity: f32) -> NreErrorCode;
    pub fn nreSetSunDirection(direction: NreVec3) -> NreErrorCode;
    pub fn nreGetSunDirection(direction: *mut NreVec3) -> NreErrorCode;

    // Scene lighting, shadows, haze, exposure and sky colour, on the bound scene
    /// Also resets shadow distance, haze and the sky to the mode's defaults.
    pub fn nreSetSceneLighting(lighting: NreSceneLighting) -> NreErrorCode;
    pub fn nreGetSceneLighting(lighting: *mut NreSceneLighting) -> NreErrorCode;
    /// Metres; 0 turns sun shadows off, negative is `NRE_ERROR_INVALID_ARGUMENT`.
    pub fn nreSetShadowDistance(meters: f32) -> NreErrorCode;
    /// `visibility`: metres, 0 = off. `height`: the haze layer's thickness, greater than 0 (renderer.h's default: 300).
    pub fn nreSetHaze(visibility: f32, height: f32) -> NreErrorCode;
    /// Greater than 0; 2 is twice as bright.
    pub fn nreSetExposure(exposure: f32) -> NreErrorCode;
    /// The background without a skybox (Small), linear 0..1.
    pub fn nreSetSkyColor(color: NreVec3) -> NreErrorCode;

    // Cubemaps and the skybox
    pub fn nreCreateCubemap(right: *const c_char, left: *const c_char, top: *const c_char, bottom: *const c_char,
                            front: *const c_char, back: *const c_char, cubemap: *mut NreCubemap) -> NreErrorCode;
    pub fn nreDestroyCubemap(cubemap: NreCubemap) -> NreErrorCode;
    /// An empty handle removes the skybox.
    pub fn nreSetSkybox(cubemap: NreCubemap) -> NreErrorCode;

    // Camera
    pub fn nreSetCameraPosition(position: NreVec3) -> NreErrorCode;
    pub fn nreGetCameraPosition(position: *mut NreVec3) -> NreErrorCode;
    pub fn nreMoveCamera(offset: NreVec3) -> NreErrorCode;
    pub fn nreMoveCameraRelative(offset: NreVec3) -> NreErrorCode;
    pub fn nreSetCameraRotation(yaw: f32, pitch: f32) -> NreErrorCode;
    pub fn nreRotateCamera(yawDelta: f32, pitchDelta: f32) -> NreErrorCode;
    pub fn nreCameraLookAt(target: NreVec3) -> NreErrorCode;
    pub fn nreGetCameraForward(forward: *mut NreVec3) -> NreErrorCode;
    pub fn nreGetViewMatrix(view: *mut NreMat4) -> NreErrorCode;
    /// Vulkan conventions: clip-space +Y down, reverse-Z with an infinite far plane. See renderer.h.
    pub fn nreGetProjectionMatrix(projection: *mut NreMat4) -> NreErrorCode;
    /// The nearest visible instance of the bound scene under a pixel (getMousePosition units). Sets `*hit`; when
    /// it's true, `*rayHit` holds the hit, otherwise `*rayHit` is zeroed. Render_3D only.
    pub fn nrePickInstance(windowPos: NreVec2, hit: *mut bool, rayHit: *mut NreRayHit) -> NreErrorCode;

    // Timing
    pub fn nreGetFPS(fps: *mut f32) -> NreErrorCode;
    pub fn nreGetDeltaTime(deltaTime: *mut f32) -> NreErrorCode;

    // Input
    pub fn nreIsMouseButtonDown(button: NreMouseButton, down: *mut bool) -> NreErrorCode;
    pub fn nreIsMouseButtonPressed(button: NreMouseButton, pressed: *mut bool) -> NreErrorCode;
    pub fn nreIsMouseButtonReleased(button: NreMouseButton, released: *mut bool) -> NreErrorCode;
    pub fn nreGetMouseDelta(delta: *mut NreVec2) -> NreErrorCode;
    pub fn nreGetScrollDelta(delta: *mut f32) -> NreErrorCode;
    pub fn nreGetMousePosition(position: *mut NreVec2) -> NreErrorCode;
    pub fn nreSetCursorMode(mode: NreCursorMode) -> NreErrorCode;
    pub fn nreIsKeyDown(key: NreKey, down: *mut bool) -> NreErrorCode;
    pub fn nreIsKeyPressed(key: NreKey, pressed: *mut bool) -> NreErrorCode;
    pub fn nreIsKeyReleased(key: NreKey, released: *mut bool) -> NreErrorCode;
}
