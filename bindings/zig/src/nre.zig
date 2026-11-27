//! Zig bindings for NRE, over its C API (include/nre.h, libnre_c).
//!
//! Same functions as renderer.h, in Zig style (NRE::createModel -> nre.createModel); see renderer.h for
//! what each one does. The raw translated header is available as `nre.c`.
//!
//! Errors: every fallible function returns an `Error` union. Zig errors carry no payload, so the details
//! live next to it: right after a call fails, `lastErrorMessage()` is the renderer's message and
//! `lastErrorCode()` the raw code (useful for codes newer than these bindings, which become `error.Unknown`):
//!
//!     const model = nre.createModel("car.glb") catch |err| {
//!         std.log.err("{t}: {s}", .{ err, nre.lastErrorMessage() });
//!         return err;
//!     };
//!
//! Handles are small values (index + generation) to copy around; destroy them explicitly, like in C++.
//! `.none` is the empty handle ("no texture", "no skybox").
//!
//! Threads: GLFW needs the window functions (setRenderConfig, init, pollEvents, input, frames, shutdown)
//! on the main thread; in practice, call NRE from the main thread only.
const std = @import("std");

/// The raw C API, translated from include/nre.h.
pub const c = @import("c");

/// The library version these bindings were written for (major.minor must match; patch may differ).
pub const abi_major = 0;
pub const abi_minor = 3;

// ---------------------------------------------------------------------------------------------
// Errors
// ---------------------------------------------------------------------------------------------

/// The C error codes. Non-exhaustive: codes added to the library later still fit (as `_`).
pub const ErrorCode = enum(c_int) {
    ok = 0,
    unsupported_gpu = 1,
    creating_vulkan_instance = 2,
    creating_swapchain = 3,
    recreating_swapchain = 4,
    creating_pipeline = 5,
    window_creation_failed = 6,
    file_not_found = 7,
    wrong_mode = 8,
    not_initialized = 9,
    already_initialized = 10,
    /// Empty, destroyed, or never created.
    invalid_handle = 11,
    unsupported_format = 12,
    model_load_failed = 13,
    failed_to_create_allocator = 14,
    buffer_creation_failed = 15,
    upload_failed = 16,
    no_pipeline_bound = 17,
    no_scene_bound = 18,
    creating_texture = 19,
    image_creation_failed = 20,
    image_view_creation_failed = 21,
    index_out_of_range = 22,
    impossible_type_for_per_frame_buffer = 23,
    buffer_with_size_null = 24,
    size_of_buffer_too_big = 25,
    descriptor_creation_failed = 26,
    shader_creation_failed = 27,
    /// Also: a C++ exception inside the renderer.
    unknown = 28,
    library_load_failed = 29,
    /// E.g. a negative shadow distance, or picking in Render_2D.
    invalid_argument = 30,
    _,

    /// "Ok", "FileNotFound", ... (from the library; a static string).
    pub fn toString(code: ErrorCode) [:0]const u8 {
        return std.mem.span(c.nreErrorCodeToString(@bitCast(@backingInt(code))));
    }

    /// The Zig error for this code; null for `.ok`. Codes these bindings don't know become `error.Unknown`.
    pub fn toError(code: ErrorCode) ?Error {
        return switch (code) {
            .ok => null,
            .unsupported_gpu => error.UnsupportedGpu,
            .creating_vulkan_instance => error.CreatingVulkanInstance,
            .creating_swapchain => error.CreatingSwapchain,
            .recreating_swapchain => error.RecreatingSwapchain,
            .creating_pipeline => error.CreatingPipeline,
            .window_creation_failed => error.WindowCreationFailed,
            .file_not_found => error.FileNotFound,
            .wrong_mode => error.WrongMode,
            .not_initialized => error.NotInitialized,
            .already_initialized => error.AlreadyInitialized,
            .invalid_handle => error.InvalidHandle,
            .unsupported_format => error.UnsupportedFormat,
            .model_load_failed => error.ModelLoadFailed,
            .failed_to_create_allocator => error.FailedToCreateAllocator,
            .buffer_creation_failed => error.BufferCreationFailed,
            .upload_failed => error.UploadFailed,
            .no_pipeline_bound => error.NoPipelineBound,
            .no_scene_bound => error.NoSceneBound,
            .creating_texture => error.CreatingTexture,
            .image_creation_failed => error.ImageCreationFailed,
            .image_view_creation_failed => error.ImageViewCreationFailed,
            .index_out_of_range => error.IndexOutOfRange,
            .impossible_type_for_per_frame_buffer => error.ImpossibleTypeForPerFrameBuffer,
            .buffer_with_size_null => error.BufferWithSizeNull,
            .size_of_buffer_too_big => error.SizeOfBufferTooBig,
            .descriptor_creation_failed => error.DescriptorCreationFailed,
            .shader_creation_failed => error.ShaderCreationFailed,
            .library_load_failed => error.LibraryLoadFailed,
            .invalid_argument => error.InvalidArgument,
            .unknown, _ => error.Unknown,
        };
    }
};

/// One error per `ErrorCode` (Unknown also covers codes newer than these bindings), plus VersionMismatch:
/// the loaded libnre_c isn't 0.3.x (see `checkVersion`; that one has no `lastErrorMessage`).
pub const Error = error{
    UnsupportedGpu,
    CreatingVulkanInstance,
    CreatingSwapchain,
    RecreatingSwapchain,
    CreatingPipeline,
    WindowCreationFailed,
    FileNotFound,
    WrongMode,
    NotInitialized,
    AlreadyInitialized,
    InvalidHandle,
    UnsupportedFormat,
    ModelLoadFailed,
    FailedToCreateAllocator,
    BufferCreationFailed,
    UploadFailed,
    NoPipelineBound,
    NoSceneBound,
    CreatingTexture,
    ImageCreationFailed,
    ImageViewCreationFailed,
    IndexOutOfRange,
    ImpossibleTypeForPerFrameBuffer,
    BufferWithSizeNull,
    SizeOfBufferTooBig,
    DescriptorCreationFailed,
    ShaderCreationFailed,
    Unknown,
    LibraryLoadFailed,
    InvalidArgument,
    VersionMismatch,
};

threadlocal var last_code: ErrorCode = .ok;

/// The message of the most recent failed call on this thread ("" if none failed yet).
/// Valid until the next failed call on this thread; copy it to keep it.
pub fn lastErrorMessage() [:0]const u8 {
    return std.mem.span(c.nreGetLastErrorMessage());
}

/// The raw code of the most recent failed call on this thread (`.ok` if none failed yet).
pub fn lastErrorCode() ErrorCode {
    return last_code;
}

/// "Ok", "FileNotFound", ...
pub fn errorCodeToString(code: ErrorCode) [:0]const u8 {
    return code.toString();
}

fn check(result: c.NreErrorCode) Error!void {
    const code: ErrorCode = @fromBackingInt(@intCast(@as(c_int, @bitCast(result))));
    if (code == .ok) return;
    last_code = code;
    return code.toError().?;
}

// ---------------------------------------------------------------------------------------------
// Version
// ---------------------------------------------------------------------------------------------

pub const Version = struct { major: u32, minor: u32, patch: u32 };

/// The loaded library's version.
pub fn version() Version {
    var v: Version = undefined;
    c.nreGetVersion(&v.major, &v.minor, &v.patch);
    return v;
}

/// error.VersionMismatch (and a log message) unless the loaded libnre_c is `abi_major.abi_minor`.x.
/// `setRenderConfig*` and `init` call it, so you only need it to check early.
pub fn checkVersion() Error!void {
    const v = version();
    if (v.major != abi_major or v.minor != abi_minor) {
        std.log.err("libnre_c is version {d}.{d}.{d}, but these bindings need {d}.{d}.x", .{ v.major, v.minor, v.patch, abi_major, abi_minor });
        return error.VersionMismatch;
    }
}

// ---------------------------------------------------------------------------------------------
// Enums (values match nre.h)
// ---------------------------------------------------------------------------------------------

pub const ShaderType = enum(c_int) { fragment = 0, vertex = 1, geometry = 2, compute = 3 };
pub const RenderMode = enum(c_int) { render_3d = 0, render_2d = 1 };
pub const ApiMode = enum(c_int) {
    /// renderFrame() draws the bound scene.
    scene = 0,
    /// raylib style: renderFrame(), drawModel()/drawInstance(), endFrame().
    function_draw = 1,
};
/// A scene's lighting mode (setSceneLighting); see renderer.h's SceneLighting. Non-exhaustive: modes added
/// to the library later still fit (as `_`).
pub const SceneLighting = enum(c_int) {
    /// Default: cubemap or sky colour, short sharp shadows, no haze.
    small = 0,
    /// Procedural sky from the sun, far cascaded shadows, haze, auto exposure.
    landscape = 1,
    _,
};
pub const RenderErrorPolicy = enum(c_int) { ignore = 0, @"return" = 1, panic = 2 };
pub const MouseButton = enum(c_int) { left = 0, right = 1, middle = 2 };
pub const CursorMode = enum(c_int) {
    normal = 0,
    /// Invisible over the window, but moves and leaves it as usual.
    hidden = 1,
    /// Hidden and kept in the window, unlimited movement: for mouse-look cameras.
    locked = 2,
};

/// Physical keys, named after the US layout. The values are GLFW's key codes.
pub const Key = enum(c_int) {
    space = 32,
    apostrophe = 39,
    comma = 44,
    minus = 45,
    period = 46,
    slash = 47,
    zero = 48,
    one = 49,
    two = 50,
    three = 51,
    four = 52,
    five = 53,
    six = 54,
    seven = 55,
    eight = 56,
    nine = 57,
    semicolon = 59,
    equal = 61,
    a = 65,
    b = 66,
    c = 67,
    d = 68,
    e = 69,
    f = 70,
    g = 71,
    h = 72,
    i = 73,
    j = 74,
    k = 75,
    l = 76,
    m = 77,
    n = 78,
    o = 79,
    p = 80,
    q = 81,
    r = 82,
    s = 83,
    t = 84,
    u = 85,
    v = 86,
    w = 87,
    x = 88,
    y = 89,
    z = 90,
    left_bracket = 91,
    backslash = 92,
    right_bracket = 93,
    grave_accent = 96,
    escape = 256,
    enter = 257,
    tab = 258,
    backspace = 259,
    insert = 260,
    delete = 261,
    right = 262,
    left = 263,
    down = 264,
    up = 265,
    page_up = 266,
    page_down = 267,
    home = 268,
    end = 269,
    caps_lock = 280,
    scroll_lock = 281,
    num_lock = 282,
    print_screen = 283,
    pause = 284,
    f1 = 290,
    f2 = 291,
    f3 = 292,
    f4 = 293,
    f5 = 294,
    f6 = 295,
    f7 = 296,
    f8 = 297,
    f9 = 298,
    f10 = 299,
    f11 = 300,
    f12 = 301,
    kp_0 = 320,
    kp_1 = 321,
    kp_2 = 322,
    kp_3 = 323,
    kp_4 = 324,
    kp_5 = 325,
    kp_6 = 326,
    kp_7 = 327,
    kp_8 = 328,
    kp_9 = 329,
    kp_decimal = 330,
    kp_divide = 331,
    kp_multiply = 332,
    kp_subtract = 333,
    kp_add = 334,
    kp_enter = 335,
    kp_equal = 336,
    left_shift = 340,
    left_control = 341,
    left_alt = 342,
    left_super = 343,
    right_shift = 344,
    right_control = 345,
    right_alt = 346,
    right_super = 347,
    menu = 348,
};

fn cEnum(e: anytype) c_uint {
    return @bitCast(@backingInt(e));
}

// ---------------------------------------------------------------------------------------------
// Math types: same layout as nre.h. Angles are in radians.
// ---------------------------------------------------------------------------------------------

pub const Vec2 = extern struct {
    x: f32 = 0,
    y: f32 = 0,

    const C = c.NreVec2;
    pub const zero: Vec2 = .{};

    pub fn init(x: f32, y: f32) Vec2 {
        return .{ .x = x, .y = y };
    }
};

pub const Vec3 = extern struct {
    x: f32 = 0,
    y: f32 = 0,
    z: f32 = 0,

    const C = c.NreVec3;
    pub const zero: Vec3 = .{};
    pub const one: Vec3 = .{ .x = 1, .y = 1, .z = 1 };

    pub fn init(x: f32, y: f32, z: f32) Vec3 {
        return .{ .x = x, .y = y, .z = z };
    }
    pub fn add(a: Vec3, b: Vec3) Vec3 {
        return .{ .x = a.x + b.x, .y = a.y + b.y, .z = a.z + b.z };
    }
    pub fn sub(a: Vec3, b: Vec3) Vec3 {
        return .{ .x = a.x - b.x, .y = a.y - b.y, .z = a.z - b.z };
    }
    pub fn scale(a: Vec3, s: f32) Vec3 {
        return .{ .x = a.x * s, .y = a.y * s, .z = a.z * s };
    }
};

/// x, y, z, w. Identity: `Quat.identity` (0, 0, 0, 1).
pub const Quat = extern struct {
    x: f32,
    y: f32,
    z: f32,
    w: f32,

    const C = c.NreQuat;
    pub const identity: Quat = .{ .x = 0, .y = 0, .z = 0, .w = 1 };
};

/// Column-major (like glm and GLSL): m[column * 4 + row].
pub const Mat4 = extern struct {
    m: [16]f32,

    const C = c.NreMat4;
    pub const identity: Mat4 = .{ .m = .{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 } };

    pub fn at(self: Mat4, column: usize, row: usize) f32 {
        return self.m[column * 4 + row];
    }
};

/// One vertex for createMeshModel. A normal of zero is computed from the triangles using the vertex.
pub const Vertex = extern struct {
    position: Vec3 = .zero,
    normal: Vec3 = .zero,
    uv: Vec2 = .zero,

    const C = c.NreVertex;
};

/// Applied as scale, then rotation, then translation. `.{}` is the identity.
pub const Transform = extern struct {
    position: Vec3 = .zero,
    rotation: Quat = .identity,
    scale: Vec3 = .one,

    const C = c.NreTransform;
    pub const identity: Transform = .{};
};

// ---------------------------------------------------------------------------------------------
// Handles. `.none` (all zeros) is the empty handle.
// ---------------------------------------------------------------------------------------------

fn Handle(comptime CHandle: type) type {
    return extern struct {
        index: u32 = 0,
        generation: u32 = 0,

        const Self = @This();
        const C = CHandle;
        pub const none: Self = .{};

        pub fn isNone(self: Self) bool {
            return self.index == 0 and self.generation == 0;
        }
        pub fn eql(a: Self, b: Self) bool {
            return a.index == b.index and a.generation == b.generation;
        }
    };
}

pub const Shader = Handle(c.NreShader);
pub const Pipeline = Handle(c.NrePipeline);
pub const Model = Handle(c.NreModel);
pub const Instance = Handle(c.NreInstance);
pub const Light = Handle(c.NreLight);
pub const Scene = Handle(c.NreScene);
pub const Texture = Handle(c.NreTexture);
pub const Material = Handle(c.NreMaterial);
pub const Cubemap = Handle(c.NreCubemap);

/// What pickInstance hit: the instance, the distance from the camera along the ray (world units) and the
/// world-space point.
pub const RayHit = extern struct {
    instance: Instance,
    distance: f32,
    point: Vec3,

    const C = c.NreRayHit;
};

/// A Zig math/handle value as its C twin (`T.C`); the layouts are checked below.
fn toC(value: anytype) @TypeOf(value).C {
    return @as(*const @TypeOf(value).C, @ptrCast(&value)).*;
}

/// The C twin of a Zig math/handle type, or the type itself (bool, f32).
fn CType(comptime T: type) type {
    return if (@typeInfo(T) == .@"struct") T.C else T;
}
fn fromC(comptime T: type, value: CType(T)) T {
    return @as(*const T, @ptrCast(&value)).*;
}

comptime {
    for (.{ Vec2, Vec3, Quat, Mat4, Vertex, Transform, Shader, Pipeline, Model, Instance, Light, Scene, Texture, Material, Cubemap, RayHit }) |T| {
        if (@sizeOf(T) != @sizeOf(T.C) or @alignOf(T) != @alignOf(T.C))
            @compileError(@typeName(T) ++ " doesn't match its nre.h layout");
        for (@typeInfo(T).@"struct".field_names) |name| {
            if (@offsetOf(T, name) != @offsetOf(T.C, name))
                @compileError(@typeName(T) ++ "." ++ name ++ " doesn't match its nre.h offset");
        }
    }
}

/// Calls a C function whose last parameter is the out-parameter, and returns that value as T.
fn get(comptime T: type, comptime f: anytype, args: anytype) Error!T {
    var out: CType(T) = undefined;
    try check(@call(.auto, f, args ++ .{&out}));
    return fromC(T, out);
}

// ---------------------------------------------------------------------------------------------
// Setup and frames
// ---------------------------------------------------------------------------------------------

/// Window config with the defaults: 3D, SceneMode, error policy Return, automatic present/submit,
/// resizable, FPS in the title. Main thread only (GLFW).
pub fn setRenderConfig(width: c_int, height: c_int, title: [:0]const u8) Error!void {
    return setRenderConfigFull(width, height, title, .render_3d, .scene, .@"return", false, true, true);
}

/// setRenderConfig with every setting. See renderer.h's RenderConfig/WindowConfig.
pub fn setRenderConfigFull(
    width: c_int,
    height: c_int,
    title: [:0]const u8,
    mode: RenderMode,
    api_mode: ApiMode,
    error_policy: RenderErrorPolicy,
    manual_frame_presentation_and_submission: bool,
    resizable: bool,
    show_fps_in_title: bool,
) Error!void {
    try checkVersion();
    return check(c.nreSetRenderConfig(cEnum(mode), cEnum(api_mode), cEnum(error_policy), manual_frame_presentation_and_submission, width, height, title.ptr, resizable, show_fps_in_title));
}

/// Opens the window and sets up Vulkan. Main thread only (GLFW).
pub fn init() Error!void {
    try checkVersion();
    return check(c.nreInit());
}
pub fn shutdown() Error!void {
    return check(c.nreShutdown());
}
/// SceneMode: draws and presents the bound scene. FunctionDrawMode: starts a frame (see renderer.h).
pub fn renderFrame() Error!void {
    return check(c.nreRenderFrame());
}
/// FunctionDrawMode only: ends the frame renderFrame() started.
pub fn endFrame() Error!void {
    return check(c.nreEndFrame());
}
pub fn presentFrame() Error!void {
    return check(c.nrePresentFrame());
}
pub fn submitFrame() Error!void {
    return check(c.nreSubmitFrame());
}
pub fn windowShouldClose() Error!bool {
    return get(bool, c.nreWindowShouldClose, .{});
}
/// Main thread only (GLFW).
pub fn pollEvents() Error!void {
    return check(c.nrePollEvents());
}

// ---------------------------------------------------------------------------------------------
// Shaders and pipelines
// ---------------------------------------------------------------------------------------------

/// Loads a compiled SPIR-V file.
pub fn createShader(path: [:0]const u8, shader_type: ShaderType) Error!Shader {
    return get(Shader, c.nreCreateShader, .{ path.ptr, cEnum(shader_type) });
}
pub fn destroyShader(shader: Shader) Error!void {
    return check(c.nreDestroyShader(toC(shader)));
}
pub fn createPipeline(vertex_shader: Shader, fragment_shader: Shader) Error!Pipeline {
    return get(Pipeline, c.nreCreatePipeline, .{ toC(vertex_shader), toC(fragment_shader) });
}
pub fn destroyPipeline(pipeline: Pipeline) Error!void {
    return check(c.nreDestroyPipeline(toC(pipeline)));
}
pub fn bindPipeline(pipeline: Pipeline) Error!void {
    return check(c.nreBindPipeline(toC(pipeline)));
}

// ---------------------------------------------------------------------------------------------
// Models and instances
// ---------------------------------------------------------------------------------------------

/// Loads a model file (glTF, OBJ, ...). It isn't drawn by itself: draw it through instances.
pub fn createModel(path: [:0]const u8) Error!Model {
    return get(Model, c.nreCreateModel, .{path.ptr});
}
/// A model from your own triangles (counter-clockwise front faces); indices are 3 per triangle, into vertices.
/// Vertices with a zero normal get one computed from their triangles. The slices are passed as-is (no copy
/// here) and copied by the renderer. null material: the default one.
pub fn createMeshModel(vertices: []const Vertex, indices: []const u32, material: ?Material) Error!Model {
    return get(Model, c.nreCreateMeshModel, .{
        @as([*c]const c.NreVertex, @ptrCast(vertices.ptr)),
        @as(u32, @intCast(vertices.len)),
        indices.ptr,
        @as(u32, @intCast(indices.len)),
        toC(material orelse Material.none),
    });
}
/// Instances of a destroyed model stay valid but draw nothing.
pub fn destroyModel(model: Model) Error!void {
    return check(c.nreDestroyModel(toC(model)));
}
pub fn createInstance(model: Model) Error!Instance {
    return get(Instance, c.nreCreateInstance, .{toC(model)});
}
/// Also removes it from every scene it's in.
pub fn destroyInstance(instance: Instance) Error!void {
    return check(c.nreDestroyInstance(toC(instance)));
}
pub fn setInstanceTransform(instance: Instance, transform: Transform) Error!void {
    const t = toC(transform);
    return check(c.nreSetInstanceTransform(toC(instance), &t));
}
pub fn getInstanceTransform(instance: Instance) Error!Transform {
    return get(Transform, c.nreGetInstanceTransform, .{toC(instance)});
}
pub fn getInstanceMatrix(instance: Instance) Error!Mat4 {
    return get(Mat4, c.nreGetInstanceMatrix, .{toC(instance)});
}
pub fn translateInstance(instance: Instance, offset: Vec3) Error!void {
    return check(c.nreTranslateInstance(toC(instance), toC(offset)));
}
/// angle in radians.
pub fn rotateInstance(instance: Instance, angle: f32, axis: Vec3) Error!void {
    return check(c.nreRotateInstance(toC(instance), angle, toC(axis)));
}
pub fn scaleInstance(instance: Instance, factor: Vec3) Error!void {
    return check(c.nreScaleInstance(toC(instance), toC(factor)));
}
/// A hidden instance stays in its scenes but isn't drawn.
pub fn setVisibility(instance: Instance, visible: bool) Error!void {
    return check(c.nreSetVisibility(toC(instance), visible));
}
/// Whether the instance casts sun shadows (it still receives them). Default true.
pub fn setCastsShadows(instance: Instance, casts_shadows: bool) Error!void {
    return check(c.nreSetCastsShadows(toC(instance), casts_shadows));
}
/// FunctionDrawMode only, between renderFrame() and endFrame(). null: the identity transform.
pub fn drawModel(model: Model, transform: ?Transform) Error!void {
    if (transform) |tr| {
        const t = toC(tr);
        return check(c.nreDrawModel(toC(model), &t));
    }
    return check(c.nreDrawModel(toC(model), null));
}
/// FunctionDrawMode only. Uses the instance's transform.
pub fn drawInstance(instance: Instance) Error!void {
    return check(c.nreDrawInstance(toC(instance)));
}

// ---------------------------------------------------------------------------------------------
// Scenes
// ---------------------------------------------------------------------------------------------

pub fn createScene() Error!Scene {
    return get(Scene, c.nreCreateScene, .{});
}
pub fn destroyScene(scene: Scene) Error!void {
    return check(c.nreDestroyScene(toC(scene)));
}
pub fn bindScene(scene: Scene) Error!void {
    return check(c.nreBindScene(toC(scene)));
}
pub fn getBoundScene() Error!Scene {
    return get(Scene, c.nreGetBoundScene, .{});
}
/// Adds to the bound scene. Adding one that's already in it does nothing.
pub fn addInstanceToScene(instance: Instance) Error!void {
    return check(c.nreAddInstanceToScene(toC(instance)));
}
pub fn removeInstanceFromScene(instance: Instance) Error!void {
    return check(c.nreRemoveInstanceFromScene(toC(instance)));
}

// ---------------------------------------------------------------------------------------------
// Textures and materials. `Texture.none` uses the default texture.
// ---------------------------------------------------------------------------------------------

/// Loads an image file (PNG, JPEG, ...) as a color (sRGB) texture: `createTextureFull(path, true)`.
pub fn createTexture(path: [:0]const u8) Error!Texture {
    return createTextureFull(path, true);
}
/// srgb: true for colour images (base colour); false for data maps (normal and metallic-roughness maps).
pub fn createTextureFull(path: [:0]const u8, srgb: bool) Error!Texture {
    return get(Texture, c.nreCreateTexture, .{ path.ptr, srgb });
}
pub fn destroyTexture(texture: Texture) Error!void {
    return check(c.nreDestroyTexture(toC(texture)));
}
/// metallic and normal are stored but not used yet (see renderer.h); pass `.none` for any of them.
pub fn createMaterial(base: Texture, metallic: Texture, normal: Texture) Error!Material {
    return get(Material, c.nreCreateMaterial, .{ toC(base), toC(metallic), toC(normal) });
}
pub fn destroyMaterial(material: Material) Error!void {
    return check(c.nreDestroyMaterial(toC(material)));
}
/// Waits for the GPU to finish the frames in flight, so don't call it every frame.
pub fn setMaterialBaseTexture(material: Material, texture: Texture) Error!void {
    return check(c.nreSetMaterialBaseTexture(toC(material), toC(texture)));
}
/// The model's material slots, in the file's order. The caller owns the slice (free it with `allocator`).
pub fn getModelMaterials(allocator: std.mem.Allocator, model: Model) (Error || std.mem.Allocator.Error)![]Material {
    var count: u32 = 0;
    try check(c.nreGetModelMaterials(toC(model), &count, null));
    const materials = try allocator.alloc(Material, count);
    errdefer allocator.free(materials);
    try check(c.nreGetModelMaterials(toC(model), &count, @ptrCast(materials.ptr)));
    return allocator.realloc(materials, count);
}
/// Makes every mesh of the model use material.
pub fn setModelMaterial(model: Model, material: Material) Error!void {
    return check(c.nreSetModelMaterial(toC(model), toC(material)));
}
/// Replaces slot index (an index into getModelMaterials).
pub fn replaceModelMaterial(model: Model, index: u32, material: Material) Error!void {
    return check(c.nreReplaceModelMaterial(toC(model), index, toC(material)));
}

// ---------------------------------------------------------------------------------------------
// Lights and the sun. null color/intensity: renderer.h's defaults below.
// ---------------------------------------------------------------------------------------------

pub const default_light_color: Vec3 = .one;
pub const default_directional_intensity: f32 = 3;
pub const default_point_intensity: f32 = 10;

/// direction is the way the light travels ({0, -1, 0} shines down). Add it to a scene to use it.
pub fn createDirectionalLight(direction: Vec3, color: ?Vec3, intensity: ?f32) Error!Light {
    return get(Light, c.nreCreateDirectionalLight, .{ toC(direction), toC(color orelse default_light_color), intensity orelse default_directional_intensity });
}
/// Falls off with 1/distance². Add it to a scene to use it.
pub fn createPointLight(position: Vec3, color: ?Vec3, intensity: ?f32) Error!Light {
    return get(Light, c.nreCreatePointLight, .{ toC(position), toC(color orelse default_light_color), intensity orelse default_point_intensity });
}
/// Also removes it from every scene it's in.
pub fn destroyLight(light: Light) Error!void {
    return check(c.nreDestroyLight(toC(light)));
}
pub fn addLightToScene(light: Light) Error!void {
    return check(c.nreAddLightToScene(toC(light)));
}
pub fn removeLightFromScene(light: Light) Error!void {
    return check(c.nreRemoveLightFromScene(toC(light)));
}
/// Only used by point lights.
pub fn setLightPosition(light: Light, position: Vec3) Error!void {
    return check(c.nreSetLightPosition(toC(light), toC(position)));
}
/// Only used by directional lights.
pub fn setLightDirection(light: Light, direction: Vec3) Error!void {
    return check(c.nreSetLightDirection(toC(light), toC(direction)));
}
pub fn setLightColor(light: Light, color: Vec3, intensity: f32) Error!void {
    return check(c.nreSetLightColor(toC(light), toC(color), intensity));
}
/// The bound scene's sun (one directional light kept apart, see renderer.h).
pub fn setSun(direction: Vec3, color: ?Vec3, intensity: ?f32) Error!void {
    return check(c.nreSetSun(toC(direction), toC(color orelse default_light_color), intensity orelse default_directional_intensity));
}
/// Keeps the color and intensity.
pub fn setSunDirection(direction: Vec3) Error!void {
    return check(c.nreSetSunDirection(toC(direction)));
}
pub fn getSunDirection() Error!Vec3 {
    return get(Vec3, c.nreGetSunDirection, .{});
}

// ---------------------------------------------------------------------------------------------
// Scene lighting, shadows, haze, exposure and sky colour, on the bound scene. renderer.h's defaults:
// Small: shadows 20 m, haze off. Landscape: shadows 500 m, haze 8000 m / 300 m. Exposure 1, sky {0.1, 0.3, 0.6}.
// ---------------------------------------------------------------------------------------------

pub const default_haze_height: f32 = 300;

/// Also resets shadow distance, haze and the sky to the mode's defaults.
pub fn setSceneLighting(lighting: SceneLighting) Error!void {
    return check(c.nreSetSceneLighting(cEnum(lighting)));
}
/// Modes newer than these bindings come back as a `_` value (not UB).
pub fn getSceneLighting() Error!SceneLighting {
    var out: c.NreSceneLighting = undefined;
    try check(c.nreGetSceneLighting(&out));
    return @fromBackingInt(@intCast(@as(c_int, @bitCast(out))));
}
/// Metres; 0 turns sun shadows off, negative is error.InvalidArgument.
pub fn setShadowDistance(meters: f32) Error!void {
    return check(c.nreSetShadowDistance(meters));
}
/// visibility: metres at which the haze hides almost everything, 0 = off. height: the haze layer's
/// thickness, greater than 0; null: `default_haze_height` (300).
pub fn setHaze(visibility: f32, height: ?f32) Error!void {
    return check(c.nreSetHaze(visibility, height orelse default_haze_height));
}
/// Greater than 0; 2 is twice as bright.
pub fn setExposure(exposure: f32) Error!void {
    return check(c.nreSetExposure(exposure));
}
/// The background without a skybox (Small), linear 0..1.
pub fn setSkyColor(color: Vec3) Error!void {
    return check(c.nreSetSkyColor(toC(color)));
}

// ---------------------------------------------------------------------------------------------
// Cubemaps and the skybox
// ---------------------------------------------------------------------------------------------

/// Six square images, OpenGL order: right (+X), left (-X), top (+Y), bottom (-Y), front (+Z), back (-Z).
pub fn createCubemap(right: [:0]const u8, left: [:0]const u8, top: [:0]const u8, bottom: [:0]const u8, front: [:0]const u8, back: [:0]const u8) Error!Cubemap {
    return get(Cubemap, c.nreCreateCubemap, .{ right.ptr, left.ptr, top.ptr, bottom.ptr, front.ptr, back.ptr });
}
/// Scenes using it go back to having no skybox.
pub fn destroyCubemap(cubemap: Cubemap) Error!void {
    return check(c.nreDestroyCubemap(toC(cubemap)));
}
/// The bound scene's sky (3D only). `Cubemap.none` removes it.
pub fn setSkybox(cubemap: Cubemap) Error!void {
    return check(c.nreSetSkybox(toC(cubemap)));
}

// ---------------------------------------------------------------------------------------------
// Camera (angles in radians)
// ---------------------------------------------------------------------------------------------

pub fn setCameraPosition(position: Vec3) Error!void {
    return check(c.nreSetCameraPosition(toC(position)));
}
pub fn getCameraPosition() Error!Vec3 {
    return get(Vec3, c.nreGetCameraPosition, .{});
}
pub fn moveCamera(offset: Vec3) Error!void {
    return check(c.nreMoveCamera(toC(offset)));
}
pub fn moveCameraRelative(offset: Vec3) Error!void {
    return check(c.nreMoveCameraRelative(toC(offset)));
}
pub fn setCameraRotation(yaw: f32, pitch: f32) Error!void {
    return check(c.nreSetCameraRotation(yaw, pitch));
}
pub fn rotateCamera(yaw_delta: f32, pitch_delta: f32) Error!void {
    return check(c.nreRotateCamera(yaw_delta, pitch_delta));
}
pub fn cameraLookAt(target: Vec3) Error!void {
    return check(c.nreCameraLookAt(toC(target)));
}
pub fn getCameraForward() Error!Vec3 {
    return get(Vec3, c.nreGetCameraForward, .{});
}
pub fn getViewMatrix() Error!Mat4 {
    return get(Mat4, c.nreGetViewMatrix, .{});
}
/// Vulkan conventions: clip-space +Y down, reverse-Z with an infinite far plane. See renderer.h.
pub fn getProjectionMatrix() Error!Mat4 {
    return get(Mat4, c.nreGetProjectionMatrix, .{});
}
/// The nearest visible instance of the bound scene under a pixel (getMousePosition units), or null.
/// Render_3D only.
pub fn pickInstance(window_pos: Vec2) Error!?RayHit {
    var hit: bool = false;
    var ray_hit: RayHit.C = undefined;
    try check(c.nrePickInstance(toC(window_pos), &hit, &ray_hit));
    return if (hit) fromC(RayHit, ray_hit) else null;
}

// ---------------------------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------------------------

/// Frames per second, averaged over the last half second.
pub fn getFps() Error!f32 {
    return get(f32, c.nreGetFPS, .{});
}
/// Seconds between the last two presented frames.
pub fn getDeltaTime() Error!f32 {
    return get(f32, c.nreGetDeltaTime, .{});
}

// ---------------------------------------------------------------------------------------------
// Input (as of the last pollEvents; pressed/released are true for one frame, see renderer.h)
// ---------------------------------------------------------------------------------------------

pub fn isMouseButtonDown(button: MouseButton) Error!bool {
    return get(bool, c.nreIsMouseButtonDown, .{cEnum(button)});
}
pub fn isMouseButtonPressed(button: MouseButton) Error!bool {
    return get(bool, c.nreIsMouseButtonPressed, .{cEnum(button)});
}
pub fn isMouseButtonReleased(button: MouseButton) Error!bool {
    return get(bool, c.nreIsMouseButtonReleased, .{cEnum(button)});
}
/// In pixels, +x right, +y down.
pub fn getMouseDelta() Error!Vec2 {
    return get(Vec2, c.nreGetMouseDelta, .{});
}
/// Scroll wheel steps; positive is up / away from you.
pub fn getScrollDelta() Error!f32 {
    return get(f32, c.nreGetScrollDelta, .{});
}
/// Pixels from the window's top-left corner.
pub fn getMousePosition() Error!Vec2 {
    return get(Vec2, c.nreGetMousePosition, .{});
}
pub fn setCursorMode(mode: CursorMode) Error!void {
    return check(c.nreSetCursorMode(cEnum(mode)));
}
pub fn isKeyDown(key: Key) Error!bool {
    return get(bool, c.nreIsKeyDown, .{cEnum(key)});
}
pub fn isKeyPressed(key: Key) Error!bool {
    return get(bool, c.nreIsKeyPressed, .{cEnum(key)});
}
pub fn isKeyReleased(key: Key) Error!bool {
    return get(bool, c.nreIsKeyReleased, .{cEnum(key)});
}
