#ifndef NRE_C_H
#define NRE_C_H

// The C API: plain C (C99 and later, also usable from C++), for C apps and other languages' FFIs
// (Python ctypes/cffi, Rust, C#, Zig, Odin, Lua, ...). Link NRE::c (libnre_c), which calls into librenderer.
// Same functions as renderer.h, named nreXxx instead of NRE::xxx; see renderer.h for what each one does.
//
// Errors: every function returns NRE_OK (0) or an NreErrorCode, and returns values through its last
// parameters. nreGetLastErrorMessage() describes the most recent failed call on this thread:
//
//     NreModel model;
//     if (nreCreateModel("box.glb", &model) != NRE_OK) {
//         fprintf(stderr, "%s\n", nreGetLastErrorMessage());
//         return 1;
//     }
//
// Pointer parameters must not be NULL unless the comment says so. String parameters are UTF-8 and only
// read during the call.
//
// Handles (NreModel, NreInstance, ...) are values to copy around; don't read or change their fields.
// A zero-initialized handle ({0}) is the empty handle, e.g. "no texture" in nreCreateMaterial or "no skybox" in nreSetSkybox.
//
// Math types: vectors are float structs, NreMat4 is column-major (like glm and GLSL), angles are in radians.

#include <stdbool.h>
#include <stdint.h>

#if defined(_WIN32)
    #ifdef NRE_C_BUILDING
        #define NRE_C_EXPORT __declspec(dllexport)
    #else
        #define NRE_C_EXPORT __declspec(dllimport)
    #endif
#else
    #define NRE_C_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// Enums. The values never change; new ones are added at the end.
// ---------------------------------------------------------------------------
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
    NRE_ERROR_INVALID_HANDLE = 11,               // empty, destroyed, or never created
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
    NRE_ERROR_UNKNOWN = 28,                      // also: a C++ exception inside the renderer
    NRE_ERROR_LIBRARY_LOAD_FAILED = 29,
    NRE_ERROR_CODE_MAX_ENUM = 0x7FFFFFFF         // keeps the enum 32-bit on every compiler
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
    NRE_CURSOR_HIDDEN = 1,   // invisible over the window, but moves and leaves it as usual
    NRE_CURSOR_LOCKED = 2,   // hidden and kept in the window, unlimited movement: for mouse-look cameras
    NRE_CURSOR_MODE_MAX_ENUM = 0x7FFFFFFF
} NreCursorMode;

// Physical keys, named after the US layout. The values are GLFW's key codes.
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

// ---------------------------------------------------------------------------
// Math types
// ---------------------------------------------------------------------------
typedef struct NreVec2 { float x, y; } NreVec2;
typedef struct NreVec3 { float x, y, z; } NreVec3;
typedef struct NreQuat { float x, y, z, w; } NreQuat;   // identity: {0, 0, 0, 1}
typedef struct NreMat4 { float m[16]; } NreMat4;        // column-major: m[column * 4 + row]

// One vertex for nreCreateMeshModel. A normal of {0,0,0} is computed from the triangles using the vertex.
typedef struct NreVertex {
    NreVec3 position;
    NreVec3 normal;
    NreVec2 uv;
} NreVertex;

// Applied as scale, then rotation, then translation. Identity: position {0,0,0}, rotation {0,0,0,1}, scale {1,1,1}.
typedef struct NreTransform {
    NreVec3 position;
    NreQuat rotation;
    NreVec3 scale;
} NreTransform;

// ---------------------------------------------------------------------------
// Handles. {0} is the empty handle.
// ---------------------------------------------------------------------------
#define NRE_DEFINE_HANDLE(name) typedef struct name { uint32_t index; uint32_t generation; } name;
NRE_DEFINE_HANDLE(NreShader)
NRE_DEFINE_HANDLE(NrePipeline)
NRE_DEFINE_HANDLE(NreModel)
NRE_DEFINE_HANDLE(NreInstance)
NRE_DEFINE_HANDLE(NreLight)
NRE_DEFINE_HANDLE(NreScene)
NRE_DEFINE_HANDLE(NreTexture)
NRE_DEFINE_HANDLE(NreMaterial)
NRE_DEFINE_HANDLE(NreCubemap)
#undef NRE_DEFINE_HANDLE

// ---------------------------------------------------------------------------
// Errors and version
// ---------------------------------------------------------------------------
// The message of the most recent failed call on this thread ("" if none failed yet).
// Stays valid until the next failed call on this thread.
NRE_C_EXPORT const char* nreGetLastErrorMessage(void);
// "Ok", "FileNotFound", ... A static string.
NRE_C_EXPORT const char* nreErrorCodeToString(NreErrorCode code);
// The library's version. Any pointer may be NULL.
NRE_C_EXPORT void nreGetVersion(uint32_t* major, uint32_t* minor, uint32_t* patch);

// ---------------------------------------------------------------------------
// Setup and frames
// ---------------------------------------------------------------------------
NRE_C_EXPORT NreErrorCode nreSetRenderConfig(NreRenderMode mode, NreApiMode apiMode, NreRenderErrorPolicy errorPolicy,
                                             bool manualFramePresentationAndSubmission,
                                             int windowWidth, int windowHeight, const char* windowTitle,
                                             bool resizable, bool showFpsInTitle);
NRE_C_EXPORT NreErrorCode nreInit(void);
NRE_C_EXPORT NreErrorCode nreShutdown(void);
NRE_C_EXPORT NreErrorCode nreRenderFrame(void);
NRE_C_EXPORT NreErrorCode nreEndFrame(void);
NRE_C_EXPORT NreErrorCode nrePresentFrame(void);
NRE_C_EXPORT NreErrorCode nreSubmitFrame(void);
NRE_C_EXPORT NreErrorCode nreWindowShouldClose(bool* shouldClose);
NRE_C_EXPORT NreErrorCode nrePollEvents(void);

// ---------------------------------------------------------------------------
// Shaders and pipelines
// ---------------------------------------------------------------------------
NRE_C_EXPORT NreErrorCode nreCreateShader(const char* shaderPath, NreShaderType type, NreShader* shader);
NRE_C_EXPORT NreErrorCode nreDestroyShader(NreShader shader);
NRE_C_EXPORT NreErrorCode nreCreatePipeline(NreShader vertexShader, NreShader fragmentShader, NrePipeline* pipeline);
NRE_C_EXPORT NreErrorCode nreDestroyPipeline(NrePipeline pipeline);
NRE_C_EXPORT NreErrorCode nreBindPipeline(NrePipeline pipeline);

// ---------------------------------------------------------------------------
// Models and instances
// ---------------------------------------------------------------------------
NRE_C_EXPORT NreErrorCode nreCreateModel(const char* modelPath, NreModel* model);
// A model from your own triangles (counter-clockwise front faces). The arrays are copied; material {0} = default.
NRE_C_EXPORT NreErrorCode nreCreateMeshModel(const NreVertex* vertices, uint32_t vertexCount, const uint32_t* indices,
                                             uint32_t indexCount, NreMaterial material, NreModel* model);
NRE_C_EXPORT NreErrorCode nreDestroyModel(NreModel model);
NRE_C_EXPORT NreErrorCode nreCreateInstance(NreModel model, NreInstance* instance);
NRE_C_EXPORT NreErrorCode nreDestroyInstance(NreInstance instance);
NRE_C_EXPORT NreErrorCode nreSetInstanceTransform(NreInstance instance, const NreTransform* transform);
NRE_C_EXPORT NreErrorCode nreGetInstanceTransform(NreInstance instance, NreTransform* transform);
NRE_C_EXPORT NreErrorCode nreGetInstanceMatrix(NreInstance instance, NreMat4* matrix);
NRE_C_EXPORT NreErrorCode nreTranslateInstance(NreInstance instance, NreVec3 offset);
NRE_C_EXPORT NreErrorCode nreRotateInstance(NreInstance instance, float angle, NreVec3 axis);
NRE_C_EXPORT NreErrorCode nreScaleInstance(NreInstance instance, NreVec3 factor);
NRE_C_EXPORT NreErrorCode nreSetVisibility(NreInstance instance, bool visible);
// transform may be NULL: the identity transform.
NRE_C_EXPORT NreErrorCode nreDrawModel(NreModel model, const NreTransform* transform);
NRE_C_EXPORT NreErrorCode nreDrawInstance(NreInstance instance);

// ---------------------------------------------------------------------------
// Scenes
// ---------------------------------------------------------------------------
NRE_C_EXPORT NreErrorCode nreCreateScene(NreScene* scene);
NRE_C_EXPORT NreErrorCode nreDestroyScene(NreScene scene);
NRE_C_EXPORT NreErrorCode nreBindScene(NreScene scene);
NRE_C_EXPORT NreErrorCode nreGetBoundScene(NreScene* scene);
NRE_C_EXPORT NreErrorCode nreAddInstanceToScene(NreInstance instance);
NRE_C_EXPORT NreErrorCode nreRemoveInstanceFromScene(NreInstance instance);

// ---------------------------------------------------------------------------
// Textures and materials. Empty texture handles ({0}) use the default texture.
// ---------------------------------------------------------------------------
NRE_C_EXPORT NreErrorCode nreCreateTexture(const char* path, NreTexture* texture);
NRE_C_EXPORT NreErrorCode nreDestroyTexture(NreTexture texture);
NRE_C_EXPORT NreErrorCode nreCreateMaterial(NreTexture baseTexture, NreTexture metallicTexture, NreTexture normalTexture,
                                            NreMaterial* material);
NRE_C_EXPORT NreErrorCode nreDestroyMaterial(NreMaterial material);
NRE_C_EXPORT NreErrorCode nreSetMaterialBaseTexture(NreMaterial material, NreTexture texture);
// The model's material slots, in two calls like Vulkan's enumerate functions: with materials NULL, sets *count to
// the number of slots. Otherwise *count is the array's size; it writes up to that many and sets *count to how many
// it wrote.
NRE_C_EXPORT NreErrorCode nreGetModelMaterials(NreModel model, uint32_t* count, NreMaterial* materials);
NRE_C_EXPORT NreErrorCode nreSetModelMaterial(NreModel model, NreMaterial material);
NRE_C_EXPORT NreErrorCode nreReplaceModelMaterial(NreModel model, uint32_t index, NreMaterial material);

// ---------------------------------------------------------------------------
// Lights and the sun. renderer.h's defaults: color {1,1,1}, intensity 3 (directional / sun) or 10 (point).
// ---------------------------------------------------------------------------
NRE_C_EXPORT NreErrorCode nreCreateDirectionalLight(NreVec3 direction, NreVec3 color, float intensity, NreLight* light);
NRE_C_EXPORT NreErrorCode nreCreatePointLight(NreVec3 position, NreVec3 color, float intensity, NreLight* light);
NRE_C_EXPORT NreErrorCode nreDestroyLight(NreLight light);
NRE_C_EXPORT NreErrorCode nreAddLightToScene(NreLight light);
NRE_C_EXPORT NreErrorCode nreRemoveLightFromScene(NreLight light);
NRE_C_EXPORT NreErrorCode nreSetLightPosition(NreLight light, NreVec3 position);
NRE_C_EXPORT NreErrorCode nreSetLightDirection(NreLight light, NreVec3 direction);
NRE_C_EXPORT NreErrorCode nreSetLightColor(NreLight light, NreVec3 color, float intensity);
NRE_C_EXPORT NreErrorCode nreSetSun(NreVec3 direction, NreVec3 color, float intensity);
NRE_C_EXPORT NreErrorCode nreSetSunDirection(NreVec3 direction);
NRE_C_EXPORT NreErrorCode nreGetSunDirection(NreVec3* direction);

// ---------------------------------------------------------------------------
// Cubemaps and the skybox
// ---------------------------------------------------------------------------
NRE_C_EXPORT NreErrorCode nreCreateCubemap(const char* right, const char* left, const char* top,
                                           const char* bottom, const char* front, const char* back,
                                           NreCubemap* cubemap);
NRE_C_EXPORT NreErrorCode nreDestroyCubemap(NreCubemap cubemap);
// An empty handle ({0}) removes the skybox.
NRE_C_EXPORT NreErrorCode nreSetSkybox(NreCubemap cubemap);

// ---------------------------------------------------------------------------
// Camera
// ---------------------------------------------------------------------------
NRE_C_EXPORT NreErrorCode nreSetCameraPosition(NreVec3 position);
NRE_C_EXPORT NreErrorCode nreGetCameraPosition(NreVec3* position);
NRE_C_EXPORT NreErrorCode nreMoveCamera(NreVec3 offset);
NRE_C_EXPORT NreErrorCode nreMoveCameraRelative(NreVec3 offset);
NRE_C_EXPORT NreErrorCode nreSetCameraRotation(float yaw, float pitch);
NRE_C_EXPORT NreErrorCode nreRotateCamera(float yawDelta, float pitchDelta);
NRE_C_EXPORT NreErrorCode nreCameraLookAt(NreVec3 target);
NRE_C_EXPORT NreErrorCode nreGetCameraForward(NreVec3* forward);
NRE_C_EXPORT NreErrorCode nreGetViewMatrix(NreMat4* view);

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------
NRE_C_EXPORT NreErrorCode nreGetFPS(float* fps);
NRE_C_EXPORT NreErrorCode nreGetDeltaTime(float* deltaTime);

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
NRE_C_EXPORT NreErrorCode nreIsMouseButtonDown(NreMouseButton button, bool* down);
NRE_C_EXPORT NreErrorCode nreIsMouseButtonPressed(NreMouseButton button, bool* pressed);
NRE_C_EXPORT NreErrorCode nreIsMouseButtonReleased(NreMouseButton button, bool* released);
NRE_C_EXPORT NreErrorCode nreGetMouseDelta(NreVec2* delta);
NRE_C_EXPORT NreErrorCode nreGetScrollDelta(float* delta);
NRE_C_EXPORT NreErrorCode nreGetMousePosition(NreVec2* position);
NRE_C_EXPORT NreErrorCode nreSetCursorMode(NreCursorMode mode);
NRE_C_EXPORT NreErrorCode nreIsKeyDown(NreKey key, bool* down);
NRE_C_EXPORT NreErrorCode nreIsKeyPressed(NreKey key, bool* pressed);
NRE_C_EXPORT NreErrorCode nreIsKeyReleased(NreKey key, bool* released);

#ifdef __cplusplus
}
#endif

#endif // NRE_C_H
