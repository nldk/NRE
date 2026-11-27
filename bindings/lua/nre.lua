-- nre.lua: LuaJIT FFI bindings for NRE's C API (include/nre.h, libnre_c).
--
--     local nre = require("nre")
--     nre.set_render_config(800, 600, "hello")
--     nre.init()
--
-- Same functions as renderer.h, in snake_case (nreCreateModel -> nre.create_model); see renderer.h for what each
-- one does. Every function raises an nre.Error table ({code, name, message}) when the C call fails; catch it with
-- pcall. Values the C API returns through out-parameters are return values here.
--
-- GLFW: call init, poll_events, window_should_close and the input functions from the main thread (the thread
-- that runs luajit). Coroutines are fine; other OS threads (lanes, effil, ...) are not.
--
-- Needs LuaJIT (ffi). Loads $NRE_LIB_DIR/libnre_c.so.0.3 when NRE_LIB_DIR is set, otherwise the library by name.

local has_ffi, ffi = pcall(require, "ffi")
if not has_ffi then
    error("nre.lua needs LuaJIT's ffi module (run it with luajit). PUC Lua would need a C module, "
        .. "which isn't included.", 0)
end

local nre = {}

-- =====================================================================================================
-- The C declarations: include/nre.h with the preprocessor parts written out (NRE_DEFINE_HANDLE expanded,
-- NRE_C_EXPORT / extern "C" / #include dropped). Keep this block in sync with nre.h; it is the only copy.
-- The enum tables (nre.Key, nre.ErrorCode, ...) are generated from it below.
-- =====================================================================================================
local CDEF = [[
typedef enum NreErrorCode {
    NRE_OK = 0,
    NRE_ERROR_UNSUPPORTED_GPU = 1,
    NRE_ERROR_CREATING_VULKAN_INSTANCE = 2,
    NRE_ERROR_CREATING_SWAPCHAIN = 3,
    NRE_ERROR_RECREATING_SWAPCHAIN = 4,
    NRE_ERROR_CREATING_PIPELINE = 5,
    NRE_ERROR_WINDOW_CREATION_FAILED = 6,
    NRE_ERROR_FILE_NOT_FOUND = 7,
    NRE_ERROR_WRONG_MODE = 8,
    NRE_ERROR_NOT_INITIALIZED = 9,
    NRE_ERROR_ALREADY_INITIALIZED = 10,
    NRE_ERROR_INVALID_HANDLE = 11,
    NRE_ERROR_UNSUPPORTED_FORMAT = 12,
    NRE_ERROR_MODEL_LOAD_FAILED = 13,
    NRE_ERROR_FAILED_TO_CREATE_ALLOCATOR = 14,
    NRE_ERROR_BUFFER_CREATION_FAILED = 15,
    NRE_ERROR_UPLOAD_FAILED = 16,
    NRE_ERROR_NO_PIPELINE_BOUND = 17,
    NRE_ERROR_NO_SCENE_BOUND = 18,
    NRE_ERROR_CREATING_TEXTURE = 19,
    NRE_ERROR_IMAGE_CREATION_FAILED = 20,
    NRE_ERROR_IMAGE_VIEW_CREATION_FAILED = 21,
    NRE_ERROR_INDEX_OUT_OF_RANGE = 22,
    NRE_ERROR_IMPOSSIBLE_TYPE_FOR_PER_FRAME_BUFFER = 23,
    NRE_ERROR_BUFFER_WITH_SIZE_NULL = 24,
    NRE_ERROR_SIZE_OF_BUFFER_TOO_BIG = 25,
    NRE_ERROR_DESCRIPTOR_CREATION_FAILED = 26,
    NRE_ERROR_SHADER_CREATION_FAILED = 27,
    NRE_ERROR_UNKNOWN = 28,
    NRE_ERROR_LIBRARY_LOAD_FAILED = 29,
    NRE_ERROR_INVALID_ARGUMENT = 30,
    NRE_ERROR_CODE_MAX_ENUM = 0x7FFFFFFF
} NreErrorCode;

typedef enum NreShaderType {
    NRE_SHADER_FRAGMENT = 0,
    NRE_SHADER_VERTEX = 1,
    NRE_SHADER_GEOMETRY = 2,
    NRE_SHADER_COMPUTE = 3,
    NRE_SHADER_TYPE_MAX_ENUM = 0x7FFFFFFF
} NreShaderType;

typedef enum NreRenderMode {
    NRE_RENDER_3D = 0,
    NRE_RENDER_2D = 1,
    NRE_RENDER_MODE_MAX_ENUM = 0x7FFFFFFF
} NreRenderMode;

typedef enum NreApiMode {
    NRE_API_MODE_SCENE = 0,
    NRE_API_MODE_FUNCTION_DRAW = 1,
    NRE_API_MODE_MAX_ENUM = 0x7FFFFFFF
} NreApiMode;

typedef enum NreSceneLighting {
    NRE_SCENE_LIGHTING_SMALL = 0,
    NRE_SCENE_LIGHTING_LANDSCAPE = 1,
    NRE_SCENE_LIGHTING_MAX_ENUM = 0x7FFFFFFF
} NreSceneLighting;

typedef enum NreRenderErrorPolicy {
    NRE_ERROR_POLICY_IGNORE = 0,
    NRE_ERROR_POLICY_RETURN = 1,
    NRE_ERROR_POLICY_PANIC = 2,
    NRE_ERROR_POLICY_MAX_ENUM = 0x7FFFFFFF
} NreRenderErrorPolicy;

typedef enum NreMouseButton {
    NRE_MOUSE_BUTTON_LEFT = 0,
    NRE_MOUSE_BUTTON_RIGHT = 1,
    NRE_MOUSE_BUTTON_MIDDLE = 2,
    NRE_MOUSE_BUTTON_MAX_ENUM = 0x7FFFFFFF
} NreMouseButton;

typedef enum NreCursorMode {
    NRE_CURSOR_NORMAL = 0,
    NRE_CURSOR_HIDDEN = 1,
    NRE_CURSOR_LOCKED = 2,
    NRE_CURSOR_MODE_MAX_ENUM = 0x7FFFFFFF
} NreCursorMode;

typedef enum NreKey {
    NRE_KEY_SPACE = 32, NRE_KEY_APOSTROPHE = 39, NRE_KEY_COMMA = 44, NRE_KEY_MINUS = 45,
    NRE_KEY_PERIOD = 46, NRE_KEY_SLASH = 47,
    NRE_KEY_0 = 48, NRE_KEY_1 = 49, NRE_KEY_2 = 50, NRE_KEY_3 = 51, NRE_KEY_4 = 52,
    NRE_KEY_5 = 53, NRE_KEY_6 = 54, NRE_KEY_7 = 55, NRE_KEY_8 = 56, NRE_KEY_9 = 57,
    NRE_KEY_SEMICOLON = 59, NRE_KEY_EQUAL = 61,
    NRE_KEY_A = 65, NRE_KEY_B = 66, NRE_KEY_C = 67, NRE_KEY_D = 68, NRE_KEY_E = 69, NRE_KEY_F = 70,
    NRE_KEY_G = 71, NRE_KEY_H = 72, NRE_KEY_I = 73, NRE_KEY_J = 74, NRE_KEY_K = 75, NRE_KEY_L = 76,
    NRE_KEY_M = 77, NRE_KEY_N = 78, NRE_KEY_O = 79, NRE_KEY_P = 80, NRE_KEY_Q = 81, NRE_KEY_R = 82,
    NRE_KEY_S = 83, NRE_KEY_T = 84, NRE_KEY_U = 85, NRE_KEY_V = 86, NRE_KEY_W = 87, NRE_KEY_X = 88,
    NRE_KEY_Y = 89, NRE_KEY_Z = 90,
    NRE_KEY_LEFT_BRACKET = 91, NRE_KEY_BACKSLASH = 92, NRE_KEY_RIGHT_BRACKET = 93, NRE_KEY_GRAVE_ACCENT = 96,
    NRE_KEY_ESCAPE = 256, NRE_KEY_ENTER = 257, NRE_KEY_TAB = 258, NRE_KEY_BACKSPACE = 259,
    NRE_KEY_INSERT = 260, NRE_KEY_DELETE = 261, NRE_KEY_RIGHT = 262, NRE_KEY_LEFT = 263,
    NRE_KEY_DOWN = 264, NRE_KEY_UP = 265, NRE_KEY_PAGE_UP = 266, NRE_KEY_PAGE_DOWN = 267,
    NRE_KEY_HOME = 268, NRE_KEY_END = 269,
    NRE_KEY_CAPS_LOCK = 280, NRE_KEY_SCROLL_LOCK = 281, NRE_KEY_NUM_LOCK = 282,
    NRE_KEY_PRINT_SCREEN = 283, NRE_KEY_PAUSE = 284,
    NRE_KEY_F1 = 290, NRE_KEY_F2 = 291, NRE_KEY_F3 = 292, NRE_KEY_F4 = 293, NRE_KEY_F5 = 294,
    NRE_KEY_F6 = 295, NRE_KEY_F7 = 296, NRE_KEY_F8 = 297, NRE_KEY_F9 = 298, NRE_KEY_F10 = 299,
    NRE_KEY_F11 = 300, NRE_KEY_F12 = 301,
    NRE_KEY_KP_0 = 320, NRE_KEY_KP_1 = 321, NRE_KEY_KP_2 = 322, NRE_KEY_KP_3 = 323, NRE_KEY_KP_4 = 324,
    NRE_KEY_KP_5 = 325, NRE_KEY_KP_6 = 326, NRE_KEY_KP_7 = 327, NRE_KEY_KP_8 = 328, NRE_KEY_KP_9 = 329,
    NRE_KEY_KP_DECIMAL = 330, NRE_KEY_KP_DIVIDE = 331, NRE_KEY_KP_MULTIPLY = 332,
    NRE_KEY_KP_SUBTRACT = 333, NRE_KEY_KP_ADD = 334, NRE_KEY_KP_ENTER = 335, NRE_KEY_KP_EQUAL = 336,
    NRE_KEY_LEFT_SHIFT = 340, NRE_KEY_LEFT_CONTROL = 341, NRE_KEY_LEFT_ALT = 342, NRE_KEY_LEFT_SUPER = 343,
    NRE_KEY_RIGHT_SHIFT = 344, NRE_KEY_RIGHT_CONTROL = 345, NRE_KEY_RIGHT_ALT = 346,
    NRE_KEY_RIGHT_SUPER = 347, NRE_KEY_MENU = 348,
    NRE_KEY_MAX_ENUM = 0x7FFFFFFF
} NreKey;

typedef struct NreVec2 { float x, y; } NreVec2;
typedef struct NreVec3 { float x, y, z; } NreVec3;
typedef struct NreQuat { float x, y, z, w; } NreQuat;
typedef struct NreMat4 { float m[16]; } NreMat4;

typedef struct NreVertex {
    NreVec3 position;
    NreVec3 normal;
    NreVec2 uv;
} NreVertex;

typedef struct NreTransform {
    NreVec3 position;
    NreQuat rotation;
    NreVec3 scale;
} NreTransform;

typedef struct NreShader { uint32_t index; uint32_t generation; } NreShader;
typedef struct NrePipeline { uint32_t index; uint32_t generation; } NrePipeline;
typedef struct NreModel { uint32_t index; uint32_t generation; } NreModel;
typedef struct NreInstance { uint32_t index; uint32_t generation; } NreInstance;
typedef struct NreLight { uint32_t index; uint32_t generation; } NreLight;
typedef struct NreScene { uint32_t index; uint32_t generation; } NreScene;
typedef struct NreTexture { uint32_t index; uint32_t generation; } NreTexture;
typedef struct NreMaterial { uint32_t index; uint32_t generation; } NreMaterial;
typedef struct NreCubemap { uint32_t index; uint32_t generation; } NreCubemap;

typedef struct NreRayHit {
    NreInstance instance;
    float distance;
    NreVec3 point;
} NreRayHit;

const char* nreGetLastErrorMessage(void);
const char* nreErrorCodeToString(NreErrorCode code);
void nreGetVersion(uint32_t* major, uint32_t* minor, uint32_t* patch);

NreErrorCode nreSetRenderConfig(NreRenderMode mode, NreApiMode apiMode, NreRenderErrorPolicy errorPolicy,
                                bool manualFramePresentationAndSubmission,
                                int windowWidth, int windowHeight, const char* windowTitle,
                                bool resizable, bool showFpsInTitle);
NreErrorCode nreInit(void);
NreErrorCode nreShutdown(void);
NreErrorCode nreRenderFrame(void);
NreErrorCode nreEndFrame(void);
NreErrorCode nrePresentFrame(void);
NreErrorCode nreSubmitFrame(void);
NreErrorCode nreWindowShouldClose(bool* shouldClose);
NreErrorCode nrePollEvents(void);

NreErrorCode nreCreateShader(const char* shaderPath, NreShaderType type, NreShader* shader);
NreErrorCode nreDestroyShader(NreShader shader);
NreErrorCode nreCreatePipeline(NreShader vertexShader, NreShader fragmentShader, NrePipeline* pipeline);
NreErrorCode nreDestroyPipeline(NrePipeline pipeline);
NreErrorCode nreBindPipeline(NrePipeline pipeline);

NreErrorCode nreCreateModel(const char* modelPath, NreModel* model);
NreErrorCode nreCreateMeshModel(const NreVertex* vertices, uint32_t vertexCount, const uint32_t* indices,
                                uint32_t indexCount, NreMaterial material, NreModel* model);
NreErrorCode nreDestroyModel(NreModel model);
NreErrorCode nreCreateInstance(NreModel model, NreInstance* instance);
NreErrorCode nreDestroyInstance(NreInstance instance);
NreErrorCode nreSetInstanceTransform(NreInstance instance, const NreTransform* transform);
NreErrorCode nreGetInstanceTransform(NreInstance instance, NreTransform* transform);
NreErrorCode nreGetInstanceMatrix(NreInstance instance, NreMat4* matrix);
NreErrorCode nreTranslateInstance(NreInstance instance, NreVec3 offset);
NreErrorCode nreRotateInstance(NreInstance instance, float angle, NreVec3 axis);
NreErrorCode nreScaleInstance(NreInstance instance, NreVec3 factor);
NreErrorCode nreSetVisibility(NreInstance instance, bool visible);
NreErrorCode nreSetCastsShadows(NreInstance instance, bool castsShadows);
NreErrorCode nreDrawModel(NreModel model, const NreTransform* transform);
NreErrorCode nreDrawInstance(NreInstance instance);

NreErrorCode nreCreateScene(NreScene* scene);
NreErrorCode nreDestroyScene(NreScene scene);
NreErrorCode nreBindScene(NreScene scene);
NreErrorCode nreGetBoundScene(NreScene* scene);
NreErrorCode nreAddInstanceToScene(NreInstance instance);
NreErrorCode nreRemoveInstanceFromScene(NreInstance instance);

NreErrorCode nreCreateTexture(const char* path, bool srgb, NreTexture* texture);
NreErrorCode nreDestroyTexture(NreTexture texture);
NreErrorCode nreCreateMaterial(NreTexture baseTexture, NreTexture metallicTexture, NreTexture normalTexture,
                               NreMaterial* material);
NreErrorCode nreDestroyMaterial(NreMaterial material);
NreErrorCode nreSetMaterialBaseTexture(NreMaterial material, NreTexture texture);
NreErrorCode nreGetModelMaterials(NreModel model, uint32_t* count, NreMaterial* materials);
NreErrorCode nreSetModelMaterial(NreModel model, NreMaterial material);
NreErrorCode nreReplaceModelMaterial(NreModel model, uint32_t index, NreMaterial material);

NreErrorCode nreCreateDirectionalLight(NreVec3 direction, NreVec3 color, float intensity, NreLight* light);
NreErrorCode nreCreatePointLight(NreVec3 position, NreVec3 color, float intensity, NreLight* light);
NreErrorCode nreDestroyLight(NreLight light);
NreErrorCode nreAddLightToScene(NreLight light);
NreErrorCode nreRemoveLightFromScene(NreLight light);
NreErrorCode nreSetLightPosition(NreLight light, NreVec3 position);
NreErrorCode nreSetLightDirection(NreLight light, NreVec3 direction);
NreErrorCode nreSetLightColor(NreLight light, NreVec3 color, float intensity);
NreErrorCode nreSetSun(NreVec3 direction, NreVec3 color, float intensity);
NreErrorCode nreSetSunDirection(NreVec3 direction);
NreErrorCode nreGetSunDirection(NreVec3* direction);

NreErrorCode nreSetSceneLighting(NreSceneLighting lighting);
NreErrorCode nreGetSceneLighting(NreSceneLighting* lighting);
NreErrorCode nreSetShadowDistance(float meters);
NreErrorCode nreSetHaze(float visibility, float height);
NreErrorCode nreSetExposure(float exposure);
NreErrorCode nreSetSkyColor(NreVec3 color);

NreErrorCode nreCreateCubemap(const char* right, const char* left, const char* top,
                              const char* bottom, const char* front, const char* back,
                              NreCubemap* cubemap);
NreErrorCode nreDestroyCubemap(NreCubemap cubemap);
NreErrorCode nreSetSkybox(NreCubemap cubemap);

NreErrorCode nreSetCameraPosition(NreVec3 position);
NreErrorCode nreGetCameraPosition(NreVec3* position);
NreErrorCode nreMoveCamera(NreVec3 offset);
NreErrorCode nreMoveCameraRelative(NreVec3 offset);
NreErrorCode nreSetCameraRotation(float yaw, float pitch);
NreErrorCode nreRotateCamera(float yawDelta, float pitchDelta);
NreErrorCode nreCameraLookAt(NreVec3 target);
NreErrorCode nreGetCameraForward(NreVec3* forward);
NreErrorCode nreGetViewMatrix(NreMat4* view);
NreErrorCode nreGetProjectionMatrix(NreMat4* projection);
NreErrorCode nrePickInstance(NreVec2 windowPos, bool* hit, NreRayHit* rayHit);

NreErrorCode nreGetFPS(float* fps);
NreErrorCode nreGetDeltaTime(float* deltaTime);

NreErrorCode nreIsMouseButtonDown(NreMouseButton button, bool* down);
NreErrorCode nreIsMouseButtonPressed(NreMouseButton button, bool* pressed);
NreErrorCode nreIsMouseButtonReleased(NreMouseButton button, bool* released);
NreErrorCode nreGetMouseDelta(NreVec2* delta);
NreErrorCode nreGetScrollDelta(float* delta);
NreErrorCode nreGetMousePosition(NreVec2* position);
NreErrorCode nreSetCursorMode(NreCursorMode mode);
NreErrorCode nreIsKeyDown(NreKey key, bool* down);
NreErrorCode nreIsKeyPressed(NreKey key, bool* pressed);
NreErrorCode nreIsKeyReleased(NreKey key, bool* released);
]]
-- =====================================================================================================

ffi.cdef(CDEF)

-- The ABI these bindings target. nre.h only adds things within a minor version, so any patch works.
local ABI_MAJOR, ABI_MINOR = 0, 3

-- -----------------------------------------------------------------------------------------------------
-- Loading the library
-- -----------------------------------------------------------------------------------------------------
local function library_name()
    if ffi.os == "Windows" then return "nre_c.dll" end
    if ffi.os == "OSX" then return "libnre_c.0.3.dylib" end
    return "libnre_c.so.0.3"
end

local function load_library()
    local name = library_name()
    local dir = os.getenv("NRE_LIB_DIR")
    local path = (dir and dir ~= "") and (dir .. "/" .. name) or name
    local ok, lib = pcall(ffi.load, path)
    if not ok then
        error(("nre: could not load %s (%s). Set NRE_LIB_DIR to the directory that holds it, "
            .. "e.g. <repo>/cmake-build-debug/bin."):format(path, tostring(lib)), 0)
    end
    return lib
end

local C = load_library()
--- The raw C library namespace (nre.lib.nreInit() etc.), for anything the wrappers don't cover.
nre.lib = C

do
    local v = ffi.new("uint32_t[3]")
    C.nreGetVersion(v, v + 1, v + 2)
    local major, minor = tonumber(v[0]), tonumber(v[1])
    if major ~= ABI_MAJOR or minor ~= ABI_MINOR then
        error(("nre: libnre_c is version %d.%d.%d, but these bindings are for %d.%d.x")
            :format(major, minor, tonumber(v[2]), ABI_MAJOR, ABI_MINOR), 0)
    end
end

-- -----------------------------------------------------------------------------------------------------
-- Enums: Lua tables of constants generated from CDEF, named without the C prefix
-- (NRE_KEY_W -> nre.Key.W, NRE_SHADER_VERTEX -> nre.ShaderType.VERTEX). The *_MAX_ENUM values are left out.
-- Functions taking an enum also accept these short names as strings ("W") and the full C names ("NRE_KEY_W").
-- -----------------------------------------------------------------------------------------------------
local ENUMS = {
    -- C type name          Lua name             prefixes to strip, first match wins
    { "NreErrorCode",         "ErrorCode",         { "NRE_ERROR_", "NRE_" } },
    { "NreShaderType",        "ShaderType",        { "NRE_SHADER_" } },
    { "NreRenderMode",        "RenderMode",        { "NRE_" } },          -- RENDER_3D, RENDER_2D
    { "NreApiMode",           "ApiMode",           { "NRE_API_MODE_" } },
    { "NreSceneLighting",     "SceneLighting",     { "NRE_SCENE_LIGHTING_" } },
    { "NreRenderErrorPolicy", "RenderErrorPolicy", { "NRE_ERROR_POLICY_" } },
    { "NreMouseButton",       "MouseButton",       { "NRE_MOUSE_BUTTON_" } },
    { "NreCursorMode",        "CursorMode",        { "NRE_CURSOR_" } },
    { "NreKey",               "Key",               { "NRE_KEY_" } },      -- Key["0"] .. Key["9"], Key.KP_0, ...
}

for _, e in ipairs(ENUMS) do
    local ctype, lua_name, prefixes = e[1], e[2], e[3]
    local body = CDEF:match("typedef enum " .. ctype .. " (%b{})")
    local tbl = {}
    for cname in body:gmatch("(NRE_[%w_]+)%s*=") do
        if not cname:match("_MAX_ENUM$") then
            local short = cname
            for _, p in ipairs(prefixes) do
                if cname:sub(1, #p) == p then short = cname:sub(#p + 1); break end
            end
            tbl[short] = tonumber(C[cname])
        end
    end
    nre[lua_name] = tbl
end

-- A number, a short name ("W") or a C name ("NRE_KEY_W", which LuaJIT converts itself).
local function enum(tbl, v)
    if type(v) == "string" then return tbl[v] or v end
    return v
end

-- -----------------------------------------------------------------------------------------------------
-- Errors
-- -----------------------------------------------------------------------------------------------------
--- The metatable of the error tables every wrapper raises: { code = <number>, name = "FileNotFound",
--- message = "<nreGetLastErrorMessage()>" }. Compare code with nre.ErrorCode.*.
local Error = {}
Error.__index = Error
Error.__tostring = function(e) return ("nre: %s (%d): %s"):format(e.name, e.code, e.message) end
nre.Error = Error

local known_code = {}
for _, v in pairs(nre.ErrorCode) do known_code[v] = true end

--- "Ok", "FileNotFound", ... (from nreErrorCodeToString). Codes this version doesn't know give "Unknown".
function nre.error_code_to_string(code)
    code = tonumber(code)
    if not known_code[code] then return "Unknown" end
    return ffi.string(C.nreErrorCodeToString(code))
end

--- The message of the most recent failed call on this thread ("" if none failed yet).
function nre.get_last_error_message()
    return ffi.string(C.nreGetLastErrorMessage())
end

--- True if v is an error table raised by these bindings.
function nre.is_error(v)
    return getmetatable(v) == Error
end

local function raise(rc, message)
    rc = tonumber(rc)
    local err = setmetatable({
        code = rc,
        name = nre.error_code_to_string(rc),
        message = message or ffi.string(C.nreGetLastErrorMessage()),
    }, Error)
    error(err, 3)
end

--- Raises an nre.Error unless rc is NRE_OK. For calls made through nre.lib directly.
function nre.check(rc)
    if rc ~= 0 then raise(rc) end
end
local check = nre.check

-- -----------------------------------------------------------------------------------------------------
-- Math types. Vec2/Vec3 have + - * / and unary -, (vector * number, number * vector, componentwise
-- vector * vector), tostring and ==. Functions taking vectors accept a cdata vector, {x, y, z} or {x=, y=, z=}.
-- LuaJIT detail: a struct field of struct type (transform.position) is a reference into its parent; copy it
-- (nre.Vec3(t.position) or nre.vec3(t.position)) before keeping it longer than the parent.
-- -----------------------------------------------------------------------------------------------------
local Vec2 = ffi.typeof("NreVec2")
local Vec3 = ffi.typeof("NreVec3")
local Quat = ffi.typeof("NreQuat")
local Mat4 = ffi.typeof("NreMat4")
local Transform = ffi.typeof("NreTransform")
local Vertex = ffi.typeof("NreVertex")
nre.Vec2, nre.Vec3, nre.Quat, nre.Mat4, nre.Transform = Vec2, Vec3, Quat, Mat4, Transform
nre.Vertex = Vertex

local sqrt = math.sqrt

local vec3_methods = {
    dot = function(a, b) return a.x * b.x + a.y * b.y + a.z * b.z end,
    cross = function(a, b) return Vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x) end,
    length = function(a) return sqrt(a.x * a.x + a.y * a.y + a.z * a.z) end,
    normalized = function(a)
        local l = sqrt(a.x * a.x + a.y * a.y + a.z * a.z)
        return l > 0 and Vec3(a.x / l, a.y / l, a.z / l) or Vec3()
    end,
    unpack = function(a) return a.x, a.y, a.z end,
}
ffi.metatype(Vec3, {
    __index = vec3_methods,
    __add = function(a, b) return Vec3(a.x + b.x, a.y + b.y, a.z + b.z) end,
    __sub = function(a, b) return Vec3(a.x - b.x, a.y - b.y, a.z - b.z) end,
    __mul = function(a, b)
        if type(a) == "number" then return Vec3(a * b.x, a * b.y, a * b.z) end
        if type(b) == "number" then return Vec3(a.x * b, a.y * b, a.z * b) end
        return Vec3(a.x * b.x, a.y * b.y, a.z * b.z)
    end,
    __div = function(a, b) return Vec3(a.x / b, a.y / b, a.z / b) end,
    __unm = function(a) return Vec3(-a.x, -a.y, -a.z) end,
    -- LuaJIT calls __eq for cdata == anything, including nil.
    __eq = function(a, b)
        return ffi.istype(Vec3, a) and ffi.istype(Vec3, b) and a.x == b.x and a.y == b.y and a.z == b.z
    end,
    __tostring = function(a) return ("Vec3(%g, %g, %g)"):format(a.x, a.y, a.z) end,
})

ffi.metatype(Vec2, {
    __index = {
        dot = function(a, b) return a.x * b.x + a.y * b.y end,
        length = function(a) return sqrt(a.x * a.x + a.y * a.y) end,
        unpack = function(a) return a.x, a.y end,
    },
    __add = function(a, b) return Vec2(a.x + b.x, a.y + b.y) end,
    __sub = function(a, b) return Vec2(a.x - b.x, a.y - b.y) end,
    __mul = function(a, b)
        if type(a) == "number" then return Vec2(a * b.x, a * b.y) end
        if type(b) == "number" then return Vec2(a.x * b, a.y * b) end
        return Vec2(a.x * b.x, a.y * b.y)
    end,
    __div = function(a, b) return Vec2(a.x / b, a.y / b) end,
    __unm = function(a) return Vec2(-a.x, -a.y) end,
    __eq = function(a, b) return ffi.istype(Vec2, a) and ffi.istype(Vec2, b) and a.x == b.x and a.y == b.y end,
    __tostring = function(a) return ("Vec2(%g, %g)"):format(a.x, a.y) end,
})

ffi.metatype(Quat, {
    __eq = function(a, b)
        return ffi.istype(Quat, a) and ffi.istype(Quat, b) and a.x == b.x and a.y == b.y and a.z == b.z and a.w == b.w
    end,
    __tostring = function(q) return ("Quat(%g, %g, %g, %g)"):format(q.x, q.y, q.z, q.w) end,
})

ffi.metatype(Mat4, {
    __index = {
        -- Column-major like nre.h: m[column * 4 + row], 0-based column and row.
        at = function(self, column, row) return self.m[column * 4 + row] end,
    },
    __tostring = function(a)
        local rows = {}
        for r = 0, 3 do
            rows[#rows + 1] = ("%g %g %g %g"):format(a.m[r], a.m[4 + r], a.m[8 + r], a.m[12 + r])
        end
        return "Mat4(" .. table.concat(rows, "; ") .. ")"
    end,
})

ffi.metatype(Transform, {
    __tostring = function(t)
        return ("Transform(position=%s, rotation=%s, scale=%s)"):format(
            tostring(t.position), tostring(t.rotation), tostring(t.scale))
    end,
})

ffi.metatype(Vertex, {
    __tostring = function(v)
        return ("Vertex(position=%s, normal=%s, uv=%s)"):format(tostring(v.position), tostring(v.normal), tostring(v.uv))
    end,
})

--- nre.vec2(x, y), or nre.vec2({x, y}) / nre.vec2({x=, y=}) / nre.vec2(vec2). Missing components are 0.
function nre.vec2(x, y)
    if ffi.istype(Vec2, x) then return Vec2(x) end
    if type(x) == "table" then return Vec2(x[1] or x.x or 0, x[2] or x.y or 0) end
    return Vec2(x or 0, y or 0)
end
-- For arguments passed straight to C: no copy needed.
local function vec2(v)
    if ffi.istype(Vec2, v) then return v end
    return nre.vec2(v)
end

--- nre.vec3(x, y, z), or nre.vec3({x, y, z}) / nre.vec3({x=, y=, z=}) / nre.vec3(vec3). Missing components are 0.
--- Always returns a new Vec3 (so nre.vec3(t.position) is a copy, not a reference into t).
function nre.vec3(x, y, z)
    if ffi.istype(Vec3, x) then return Vec3(x) end
    if type(x) == "table" then return Vec3(x[1] or x.x or 0, x[2] or x.y or 0, x[3] or x.z or 0) end
    return Vec3(x or 0, y or 0, z or 0)
end
-- For arguments passed straight to C: no copy needed.
local function vec3(v)
    if ffi.istype(Vec3, v) then return v end
    return nre.vec3(v)
end

--- nre.quat(x, y, z, w); no arguments is the identity (0, 0, 0, 1). Also takes a table or a Quat.
function nre.quat(x, y, z, w)
    if ffi.istype(Quat, x) then return Quat(x) end
    if type(x) == "table" then return Quat(x[1] or x.x or 0, x[2] or x.y or 0, x[3] or x.z or 0, x[4] or x.w or 1) end
    if x == nil then return Quat(0, 0, 0, 1) end
    return Quat(x, y or 0, z or 0, w or 1)
end
local quat = nre.quat

--- nre.transform(position, rotation, scale): each part optional (identity: {0,0,0}, {0,0,0,1}, {1,1,1}).
--- Also takes a table {position=, rotation=, scale=} or a Transform.
function nre.transform(position, rotation, scale)
    if ffi.istype(Transform, position) then return Transform(position) end
    if type(position) == "table" and (position.position or position.rotation or position.scale) then
        position, rotation, scale = position.position, position.rotation, position.scale
    end
    local t = Transform()
    if position ~= nil then t.position = vec3(position) end
    t.rotation = quat(rotation)
    if scale ~= nil then t.scale = vec3(scale) else t.scale = Vec3(1, 1, 1) end
    return t
end
-- For arguments passed straight to C: no copy needed.
local function transform(t)
    if ffi.istype(Transform, t) then return t end
    return nre.transform(t)
end

-- Writes one vertex given as anything nre.vertex accepts into the cdata Vertex dst.
local function set_vertex(dst, position, normal, uv)
    if ffi.istype(Vertex, position) then dst.position, dst.normal, dst.uv = position.position, position.normal, position.uv; return end
    if type(position) == "table" and not ffi.istype(Vec3, position) then
        if position.position ~= nil or position.normal ~= nil or position.uv ~= nil then
            position, normal, uv = position.position, position.normal, position.uv
        elseif type(position[1]) == "table" or type(position[1]) == "cdata" then
            position, normal, uv = position[1], position[2], position[3]
        end
    end
    if position ~= nil then dst.position = vec3(position) end
    if normal ~= nil then dst.normal = vec3(normal) end
    if uv ~= nil then dst.uv = vec2(uv) end
end

--- nre.vertex(position, normal, uv), each optional (0 if missing; a zero normal is computed by
--- create_mesh_model). Also takes {position=, normal=, uv=}, {{x,y,z}, {x,y,z}, {u,v}} or a Vertex.
function nre.vertex(position, normal, uv)
    local v = Vertex()
    set_vertex(v, position, normal, uv)
    return v
end

-- -----------------------------------------------------------------------------------------------------
-- Handles: cdata structs {index, generation}, copied by value. Calling the type gives the empty handle
-- (nre.Texture() is "no texture"), and so does nil wherever the C API gives {0} a meaning.
-- No finalizers: destroy GPU objects yourself, like in C++.
-- -----------------------------------------------------------------------------------------------------
local HANDLES = { "Shader", "Pipeline", "Model", "Instance", "Light", "Scene", "Texture", "Material", "Cubemap" }
for _, name in ipairs(HANDLES) do
    local ct = ffi.typeof("Nre" .. name)
    ffi.metatype(ct, {
        __index = {
            is_empty = function(h) return h.index == 0 and h.generation == 0 end,
        },
        __eq = function(a, b)
            return ffi.istype(ct, a) and ffi.istype(ct, b) and a.index == b.index and a.generation == b.generation
        end,
        __tostring = function(h) return ("%s(%d, %d)"):format(name, h.index, h.generation) end,
    })
    nre[name] = ct
end
local Shader, Pipeline, Model, Instance = nre.Shader, nre.Pipeline, nre.Model, nre.Instance
local Light, Scene, Texture, Material, Cubemap = nre.Light, nre.Scene, nre.Texture, nre.Material, nre.Cubemap

local EMPTY_TEXTURE, EMPTY_CUBEMAP, EMPTY_MATERIAL = Texture(), Cubemap(), Material()

--- What pick_instance hit: { instance = Instance, distance = <world units from the camera>, point = Vec3 }.
local RayHit = ffi.typeof("NreRayHit")
ffi.metatype(RayHit, {
    __tostring = function(h)
        return ("RayHit(instance=%s, distance=%g, point=%s)"):format(tostring(h.instance), h.distance, tostring(h.point))
    end,
})
nre.RayHit = RayHit

-- Scratch out-parameters for values returned as Lua numbers/booleans (structs get a fresh cdata each call).
local out_bool = ffi.new("bool[1]")
local out_float = ffi.new("float[1]")
local out_count = ffi.new("uint32_t[1]")

local function get_bool(fn, arg)
    check(fn(arg, out_bool))
    return out_bool[0]
end

local function get_float(fn)
    check(fn(out_float))
    return out_float[0]
end

local function get_new(ct, fn)
    local v = ct()
    check(fn(v))
    return v
end

-- =====================================================================================================
-- Setup and frames (see renderer.h)
-- =====================================================================================================

--- Returns major, minor, patch of the loaded libnre_c.
function nre.get_version()
    local v = ffi.new("uint32_t[3]")
    C.nreGetVersion(v, v + 1, v + 2)
    return tonumber(v[0]), tonumber(v[1]), tonumber(v[2])
end

--- Call before init. Plain parameters with defaults: mode = RENDER_3D, api_mode = SCENE, error_policy = RETURN,
--- manual_frame_presentation_and_submission = false, resizable = true, show_fps_in_title = true.
function nre.set_render_config(width, height, title, mode, api_mode, error_policy,
                               manual_frame_presentation_and_submission, resizable, show_fps_in_title)
    if mode == nil then mode = nre.RenderMode.RENDER_3D end
    if api_mode == nil then api_mode = nre.ApiMode.SCENE end
    if error_policy == nil then error_policy = nre.RenderErrorPolicy.RETURN end
    if manual_frame_presentation_and_submission == nil then manual_frame_presentation_and_submission = false end
    if resizable == nil then resizable = true end
    if show_fps_in_title == nil then show_fps_in_title = true end
    check(C.nreSetRenderConfig(enum(nre.RenderMode, mode), enum(nre.ApiMode, api_mode),
        enum(nre.RenderErrorPolicy, error_policy), manual_frame_presentation_and_submission,
        width, height, title, resizable, show_fps_in_title))
end

--- Opens the window and sets up Vulkan. Main thread only (GLFW).
function nre.init() check(C.nreInit()) end
function nre.shutdown() check(C.nreShutdown()) end
--- SceneMode: draws and presents the bound scene. FunctionDrawMode: starts the frame (then draw_*, then end_frame).
function nre.render_frame() check(C.nreRenderFrame()) end
function nre.end_frame() check(C.nreEndFrame()) end
function nre.present_frame() check(C.nrePresentFrame()) end
function nre.submit_frame() check(C.nreSubmitFrame()) end
--- Returns a boolean. Main thread only (GLFW).
function nre.window_should_close()
    check(C.nreWindowShouldClose(out_bool))
    return out_bool[0]
end
--- Main thread only (GLFW).
function nre.poll_events() check(C.nrePollEvents()) end

-- =====================================================================================================
-- Shaders and pipelines
-- =====================================================================================================

--- type: nre.ShaderType.VERTEX / FRAGMENT / ... (or "VERTEX"). Returns a Shader.
function nre.create_shader(shader_path, shader_type)
    local h = Shader()
    check(C.nreCreateShader(shader_path, enum(nre.ShaderType, shader_type), h))
    return h
end
function nre.destroy_shader(shader) check(C.nreDestroyShader(shader)) end
function nre.create_pipeline(vertex_shader, fragment_shader)
    local h = Pipeline()
    check(C.nreCreatePipeline(vertex_shader, fragment_shader, h))
    return h
end
function nre.destroy_pipeline(pipeline) check(C.nreDestroyPipeline(pipeline)) end
function nre.bind_pipeline(pipeline) check(C.nreBindPipeline(pipeline)) end

-- =====================================================================================================
-- Models and instances
-- =====================================================================================================

--- Loads a model file (glTF/GLB, OBJ). Draw it through instances (create_instance) or draw_model.
function nre.create_model(model_path)
    local h = Model()
    check(C.nreCreateModel(model_path, h))
    return h
end

local VertexArray = ffi.typeof("NreVertex[?]")
-- For error messages: cdata by its type (tostring would run a metatype's __tostring, e.g. through a NULL pointer).
local function describe(v)
    if type(v) == "cdata" then return tostring(ffi.typeof(v)) end
    return type(v) .. " " .. tostring(v)
end
local IndexArray = ffi.typeof("uint32_t[?]")

-- The element count of a cdata array (fixed-size or [?], const or not) of the element type elem, or nil for
-- anything else, pointers included (they carry no count). ffi.istype only matches one exact array type, so this
-- compares LuaJIT's type names: "ctype<struct NreVertex [3]>", "ctype<const unsigned int [?]>".
local function cdata_array_count(v, elem)
    if type(v) ~= "cdata" then return nil end
    local inner = tostring(ffi.typeof(elem)):match("^ctype<(.*)>$")
    local name = tostring(ffi.typeof(v)):match("^ctype<(.*) %[[%d?]+%]>$")
    if name ~= inner and name ~= "const " .. inner then return nil end
    return ffi.sizeof(v) / ffi.sizeof(elem)
end

--- A model from your own triangles (counter-clockwise front faces); see renderer.h's createMeshModel.
--- vertices: a Lua list of anything nre.vertex accepts ({position=, normal=, uv=}, {{x,y,z}, {x,y,z}, {u,v}},
---   a Vertex), or a cdata NreVertex array (ffi.new("NreVertex[?]", n)), passed to C without a copy.
--- indices: a Lua list of 1-BASED vertex indices (vertices[1] is index 1; subtracted by 1 for C), or a cdata
---   uint32_t array of 0-BASED indices, passed to C as is. Three per triangle.
--- material nil = the default material. The geometry is copied; the arrays can be freed afterwards.
function nre.create_mesh_model(vertices, indices, material)
    local vcount = cdata_array_count(vertices, "NreVertex")
    local varr = vertices
    if vcount == nil then
        if type(vertices) ~= "table" then
            raise(nre.ErrorCode.INVALID_ARGUMENT, "create_mesh_model: vertices must be a Lua list or an NreVertex[] "
                .. "cdata array, got " .. describe(vertices))
        end
        vcount = #vertices
        varr = VertexArray(vcount)
        for i = 1, vcount do set_vertex(varr[i - 1], vertices[i]) end
    end

    local icount = cdata_array_count(indices, "uint32_t")
    local iarr = indices
    if icount == nil then
        if type(indices) ~= "table" then
            raise(nre.ErrorCode.INVALID_ARGUMENT, "create_mesh_model: indices must be a Lua list or a uint32_t[] "
                .. "cdata array, got " .. describe(indices))
        end
        icount = #indices
        iarr = IndexArray(icount)
        for i = 1, icount do
            local index = indices[i]
            if type(index) ~= "number" or index < 1 or index % 1 ~= 0 then
                raise(nre.ErrorCode.INVALID_ARGUMENT, ("create_mesh_model: indices[%d] must be an integer >= 1 "
                    .. "(Lua lists are 1-based), got %s"):format(i, describe(index)))
            end
            iarr[i - 1] = index - 1
        end
    end

    local h = Model()
    check(C.nreCreateMeshModel(varr, vcount, iarr, icount, material or EMPTY_MATERIAL, h))
    return h
end
function nre.destroy_model(model) check(C.nreDestroyModel(model)) end
function nre.create_instance(model)
    local h = Instance()
    check(C.nreCreateInstance(model, h))
    return h
end
function nre.destroy_instance(instance) check(C.nreDestroyInstance(instance)) end
--- transform: a Transform or anything nre.transform accepts.
function nre.set_instance_transform(instance, t) check(C.nreSetInstanceTransform(instance, transform(t))) end
--- Returns a Transform.
function nre.get_instance_transform(instance)
    local t = Transform()
    check(C.nreGetInstanceTransform(instance, t))
    return t
end
--- Returns a Mat4 (column-major).
function nre.get_instance_matrix(instance)
    local m = Mat4()
    check(C.nreGetInstanceMatrix(instance, m))
    return m
end
function nre.translate_instance(instance, offset) check(C.nreTranslateInstance(instance, vec3(offset))) end
--- angle in radians.
function nre.rotate_instance(instance, angle, axis) check(C.nreRotateInstance(instance, angle, vec3(axis))) end
function nre.scale_instance(instance, factor) check(C.nreScaleInstance(instance, vec3(factor))) end
function nre.set_visibility(instance, visible) check(C.nreSetVisibility(instance, visible and true or false)) end
--- Whether the instance casts sun shadows (it still receives them). Default true.
function nre.set_casts_shadows(instance, casts_shadows)
    check(C.nreSetCastsShadows(instance, casts_shadows and true or false))
end
--- FunctionDrawMode, between render_frame and end_frame. transform nil = identity.
function nre.draw_model(model, t)
    check(C.nreDrawModel(model, t ~= nil and transform(t) or nil))
end
function nre.draw_instance(instance) check(C.nreDrawInstance(instance)) end

-- =====================================================================================================
-- Scenes
-- =====================================================================================================
function nre.create_scene() return get_new(Scene, C.nreCreateScene) end
function nre.destroy_scene(scene) check(C.nreDestroyScene(scene)) end
function nre.bind_scene(scene) check(C.nreBindScene(scene)) end
function nre.get_bound_scene() return get_new(Scene, C.nreGetBoundScene) end
--- Adds to / removes from the bound scene.
function nre.add_instance_to_scene(instance) check(C.nreAddInstanceToScene(instance)) end
function nre.remove_instance_from_scene(instance) check(C.nreRemoveInstanceFromScene(instance)) end

-- =====================================================================================================
-- Textures and materials. A nil texture means the empty handle: the default texture.
-- =====================================================================================================
--- srgb (default true): true for colour images (base colour), false for data maps (normal, metallic-roughness).
function nre.create_texture(path, srgb)
    if srgb == nil then srgb = true end
    local h = Texture()
    check(C.nreCreateTexture(path, srgb and true or false, h))
    return h
end
function nre.destroy_texture(texture) check(C.nreDestroyTexture(texture)) end
--- All three optional (nil = default texture). metallic and normal are stored but not used yet (see renderer.h).
function nre.create_material(base_texture, metallic_texture, normal_texture)
    local h = Material()
    check(C.nreCreateMaterial(base_texture or EMPTY_TEXTURE, metallic_texture or EMPTY_TEXTURE,
        normal_texture or EMPTY_TEXTURE, h))
    return h
end
function nre.destroy_material(material) check(C.nreDestroyMaterial(material)) end
--- texture nil = default texture. Waits for the GPU, so not every frame.
function nre.set_material_base_texture(material, texture)
    check(C.nreSetMaterialBaseTexture(material, texture or EMPTY_TEXTURE))
end
--- Returns a Lua list (1-based) of the model's Material slots, in the file's order. Empty without materials.
function nre.get_model_materials(model)
    check(C.nreGetModelMaterials(model, out_count, nil))
    local n = out_count[0]
    local list = {}
    if n == 0 then return list end
    local arr = ffi.new("NreMaterial[?]", n)
    out_count[0] = n
    check(C.nreGetModelMaterials(model, out_count, arr))
    for i = 0, out_count[0] - 1 do list[i + 1] = Material(arr[i]) end
    return list
end
function nre.set_model_material(model, material) check(C.nreSetModelMaterial(model, material)) end
--- index is 1-based, an index into the list get_model_materials returns (the C API's index + 1).
function nre.replace_model_material(model, index, material)
    if type(index) ~= "number" or index < 1 or index % 1 ~= 0 then
        raise(nre.ErrorCode.INDEX_OUT_OF_RANGE,
            ("replace_model_material: index must be an integer >= 1 (1-based), got %s"):format(tostring(index)))
    end
    check(C.nreReplaceModelMaterial(model, index - 1, material))
end

-- =====================================================================================================
-- Lights and the sun. Defaults like renderer.h: color {1,1,1}, intensity 3 (directional / sun) or 10 (point).
-- direction is the way the light travels ({0, -1, 0} shines straight down).
-- =====================================================================================================
local WHITE = Vec3(1, 1, 1)

function nre.create_directional_light(direction, color, intensity)
    local h = Light()
    check(C.nreCreateDirectionalLight(vec3(direction), color ~= nil and vec3(color) or WHITE,
        intensity or 3.0, h))
    return h
end
function nre.create_point_light(position, color, intensity)
    local h = Light()
    check(C.nreCreatePointLight(vec3(position), color ~= nil and vec3(color) or WHITE, intensity or 10.0, h))
    return h
end
function nre.destroy_light(light) check(C.nreDestroyLight(light)) end
function nre.add_light_to_scene(light) check(C.nreAddLightToScene(light)) end
function nre.remove_light_from_scene(light) check(C.nreRemoveLightFromScene(light)) end
--- Point lights only.
function nre.set_light_position(light, position) check(C.nreSetLightPosition(light, vec3(position))) end
--- Directional lights only.
function nre.set_light_direction(light, direction) check(C.nreSetLightDirection(light, vec3(direction))) end
function nre.set_light_color(light, color, intensity) check(C.nreSetLightColor(light, vec3(color), intensity)) end
function nre.set_sun(direction, color, intensity)
    check(C.nreSetSun(vec3(direction), color ~= nil and vec3(color) or WHITE, intensity or 3.0))
end
function nre.set_sun_direction(direction) check(C.nreSetSunDirection(vec3(direction))) end
--- Returns a Vec3.
function nre.get_sun_direction() return get_new(Vec3, C.nreGetSunDirection) end

-- =====================================================================================================
-- Scene lighting, shadows, haze, exposure and sky colour, on the bound scene. Defaults like renderer.h:
-- Small: shadows 20 m, haze off. Landscape: shadows 500 m, haze 8000 m / 300 m. Exposure 1, sky {0.1, 0.3, 0.6}.
-- =====================================================================================================
--- lighting: nre.SceneLighting.SMALL / LANDSCAPE (or "LANDSCAPE"). Also resets shadow distance, haze and the sky.
function nre.set_scene_lighting(lighting) check(C.nreSetSceneLighting(enum(nre.SceneLighting, lighting))) end
local out_lighting = ffi.new("NreSceneLighting[1]")
--- Returns a number: compare with nre.SceneLighting.* (a value newer than these bindings comes back as is).
function nre.get_scene_lighting()
    check(C.nreGetSceneLighting(out_lighting))
    return tonumber(out_lighting[0])
end
--- Metres; 0 turns sun shadows off, negative raises INVALID_ARGUMENT.
function nre.set_shadow_distance(meters) check(C.nreSetShadowDistance(meters)) end
--- visibility: metres at which the haze hides almost everything, 0 = off. height (default 300): the layer's thickness.
function nre.set_haze(visibility, height) check(C.nreSetHaze(visibility, height or 300.0)) end
--- Greater than 0; 2 is twice as bright.
function nre.set_exposure(exposure) check(C.nreSetExposure(exposure)) end
--- The background without a skybox (Small), linear 0..1.
function nre.set_sky_color(color) check(C.nreSetSkyColor(vec3(color))) end

-- =====================================================================================================
-- Cubemaps and the skybox
-- =====================================================================================================
--- Six image paths in the OpenGL order: right (+X), left (-X), top (+Y), bottom (-Y), front (+Z), back (-Z).
function nre.create_cubemap(right, left, top, bottom, front, back)
    local h = Cubemap()
    check(C.nreCreateCubemap(right, left, top, bottom, front, back, h))
    return h
end
function nre.destroy_cubemap(cubemap) check(C.nreDestroyCubemap(cubemap)) end
--- nil (or an empty Cubemap) removes the skybox.
function nre.set_skybox(cubemap) check(C.nreSetSkybox(cubemap or EMPTY_CUBEMAP)) end

-- =====================================================================================================
-- Camera. Angles in radians.
-- =====================================================================================================
function nre.set_camera_position(position) check(C.nreSetCameraPosition(vec3(position))) end
function nre.get_camera_position() return get_new(Vec3, C.nreGetCameraPosition) end
function nre.move_camera(offset) check(C.nreMoveCamera(vec3(offset))) end
function nre.move_camera_relative(offset) check(C.nreMoveCameraRelative(vec3(offset))) end
function nre.set_camera_rotation(yaw, pitch) check(C.nreSetCameraRotation(yaw, pitch)) end
function nre.rotate_camera(yaw_delta, pitch_delta) check(C.nreRotateCamera(yaw_delta, pitch_delta)) end
function nre.camera_look_at(target) check(C.nreCameraLookAt(vec3(target))) end
function nre.get_camera_forward() return get_new(Vec3, C.nreGetCameraForward) end
function nre.get_view_matrix() return get_new(Mat4, C.nreGetViewMatrix) end
--- Vulkan conventions: clip-space +Y down, reverse-Z with an infinite far plane (see renderer.h).
function nre.get_projection_matrix() return get_new(Mat4, C.nreGetProjectionMatrix) end
--- The nearest visible instance of the bound scene under a pixel (get_mouse_position units): a new RayHit
--- ({instance, distance, point}), or nil when nothing is hit. Render_3D only.
function nre.pick_instance(window_pos)
    local hit = RayHit()
    check(C.nrePickInstance(vec2(window_pos), out_bool, hit))
    if not out_bool[0] then return nil end
    return hit
end

-- =====================================================================================================
-- Timing
-- =====================================================================================================
function nre.get_fps() return get_float(C.nreGetFPS) end
--- Seconds between the last two presented frames.
function nre.get_delta_time() return get_float(C.nreGetDeltaTime) end

-- =====================================================================================================
-- Input (main thread only, GLFW). Deltas cover the time between the last two poll_events calls.
-- button: nre.MouseButton.LEFT (or "LEFT"); key: nre.Key.W (or "W"). Booleans and Vec2s are returned.
-- =====================================================================================================
function nre.is_mouse_button_down(button) return get_bool(C.nreIsMouseButtonDown, enum(nre.MouseButton, button)) end
function nre.is_mouse_button_pressed(button)
    return get_bool(C.nreIsMouseButtonPressed, enum(nre.MouseButton, button))
end
function nre.is_mouse_button_released(button)
    return get_bool(C.nreIsMouseButtonReleased, enum(nre.MouseButton, button))
end
--- Pixels; +x right, +y down.
function nre.get_mouse_delta() return get_new(Vec2, C.nreGetMouseDelta) end
--- Scroll steps; positive is up / away from you.
function nre.get_scroll_delta() return get_float(C.nreGetScrollDelta) end
function nre.get_mouse_position() return get_new(Vec2, C.nreGetMousePosition) end
function nre.set_cursor_mode(mode) check(C.nreSetCursorMode(enum(nre.CursorMode, mode))) end
function nre.is_key_down(key) return get_bool(C.nreIsKeyDown, enum(nre.Key, key)) end
function nre.is_key_pressed(key) return get_bool(C.nreIsKeyPressed, enum(nre.Key, key)) end
function nre.is_key_released(key) return get_bool(C.nreIsKeyReleased, enum(nre.Key, key)) end

return nre
