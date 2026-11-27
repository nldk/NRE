// libnre_c: the C API in nre.h, as thin wrappers over the C++ API in renderer.h.
// Each wrapper converts its arguments, calls NRE::xxx, stores the value through the out-parameter and turns
// an Error into an NreErrorCode plus the thread's last error message. No C++ exception leaves this file.

#include "nre.h"

#include <cstddef>
#include <cstring>
#include <exception>
#include <string>
#include <utility>

#include <glm/gtc/type_ptr.hpp>

#include "renderer.h"
#include "renderer_version.h"

namespace {
    // ---------------------------------------------------------------------------------------------
    // Enums: the C values are the C++ ones (error codes: +1, since 0 is NRE_OK), so they're just casts.
    // ---------------------------------------------------------------------------------------------
    constexpr NreErrorCode toC(NRE::ErrorCode code) { return static_cast<NreErrorCode>(static_cast<int>(code) + 1); }
    constexpr NRE::ErrorCode toCpp(NreErrorCode code) { return static_cast<NRE::ErrorCode>(static_cast<int>(code) - 1); }

#define NRE_CHECK_ERROR(c, cpp) static_assert(toC(NRE::ErrorCode::cpp) == c, #c " doesn't match ErrorCode::" #cpp);
    NRE_CHECK_ERROR(NRE_ERROR_UNSUPPORTED_GPU, UnsupportedGPU)
    NRE_CHECK_ERROR(NRE_ERROR_CREATING_VULKAN_INSTANCE, ErrorCreatingVulkanInstance)
    NRE_CHECK_ERROR(NRE_ERROR_CREATING_SWAPCHAIN, ErrorCreatingSwapchain)
    NRE_CHECK_ERROR(NRE_ERROR_RECREATING_SWAPCHAIN, ErrorRecreatingSwapchain)
    NRE_CHECK_ERROR(NRE_ERROR_CREATING_PIPELINE, ErrorCreatingPipeline)
    NRE_CHECK_ERROR(NRE_ERROR_WINDOW_CREATION_FAILED, WindowCreationFailed)
    NRE_CHECK_ERROR(NRE_ERROR_FILE_NOT_FOUND, FileNotFound)
    NRE_CHECK_ERROR(NRE_ERROR_WRONG_MODE, WrongMode)
    NRE_CHECK_ERROR(NRE_ERROR_NOT_INITIALIZED, NotInitialized)
    NRE_CHECK_ERROR(NRE_ERROR_ALREADY_INITIALIZED, AlreadyInitialized)
    NRE_CHECK_ERROR(NRE_ERROR_INVALID_HANDLE, HandelOutOfDateOrNonExistend)
    NRE_CHECK_ERROR(NRE_ERROR_UNSUPPORTED_FORMAT, UnsupportedFormat)
    NRE_CHECK_ERROR(NRE_ERROR_MODEL_LOAD_FAILED, ModelLoadFailed)
    NRE_CHECK_ERROR(NRE_ERROR_FAILED_TO_CREATE_ALLOCATOR, FailedToCreateAllocator)
    NRE_CHECK_ERROR(NRE_ERROR_BUFFER_CREATION_FAILED, BufferCreationFailed)
    NRE_CHECK_ERROR(NRE_ERROR_UPLOAD_FAILED, UploadFailed)
    NRE_CHECK_ERROR(NRE_ERROR_NO_PIPELINE_BOUND, NoPipelineBound)
    NRE_CHECK_ERROR(NRE_ERROR_NO_SCENE_BOUND, NoSceneBound)
    NRE_CHECK_ERROR(NRE_ERROR_CREATING_TEXTURE, ErrorCreatingTexture)
    NRE_CHECK_ERROR(NRE_ERROR_IMAGE_CREATION_FAILED, ImageCreationFailed)
    NRE_CHECK_ERROR(NRE_ERROR_IMAGE_VIEW_CREATION_FAILED, ImageViewCreationFailed)
    NRE_CHECK_ERROR(NRE_ERROR_INDEX_OUT_OF_RANGE, IndexOutOfRange)
    NRE_CHECK_ERROR(NRE_ERROR_IMPOSSIBLE_TYPE_FOR_PER_FRAME_BUFFER, ImpossibleTypeForPerFrameBuffer)
    NRE_CHECK_ERROR(NRE_ERROR_BUFFER_WITH_SIZE_NULL, BufferWithSizeNull)
    NRE_CHECK_ERROR(NRE_ERROR_SIZE_OF_BUFFER_TOO_BIG, SizeOfBufferToBig)
    NRE_CHECK_ERROR(NRE_ERROR_DESCRIPTOR_CREATION_FAILED, DescriptorCreationFailed)
    NRE_CHECK_ERROR(NRE_ERROR_SHADER_CREATION_FAILED, ShaderCreationFailed)
    NRE_CHECK_ERROR(NRE_ERROR_UNKNOWN, UnknownError)
    NRE_CHECK_ERROR(NRE_ERROR_LIBRARY_LOAD_FAILED, LibraryLoadFailed)
#undef NRE_CHECK_ERROR

#define NRE_CHECK_ENUM(c, cpp) static_assert(static_cast<int>(c) == static_cast<int>(cpp), #c " doesn't match " #cpp);
    NRE_CHECK_ENUM(NRE_SHADER_FRAGMENT, NRE::ShaderType::FRAGMENT_SHADER)
    NRE_CHECK_ENUM(NRE_SHADER_VERTEX, NRE::ShaderType::VERTEX_SHADER)
    NRE_CHECK_ENUM(NRE_SHADER_GEOMETRY, NRE::ShaderType::GEOMETRY_SHADER)
    NRE_CHECK_ENUM(NRE_SHADER_COMPUTE, NRE::ShaderType::COMPUTE_SHADER)
    NRE_CHECK_ENUM(NRE_RENDER_3D, NRE::RenderMode::Render_3D)
    NRE_CHECK_ENUM(NRE_RENDER_2D, NRE::RenderMode::Render_2D)
    NRE_CHECK_ENUM(NRE_API_MODE_SCENE, NRE::ApiMode::SceneMode)
    NRE_CHECK_ENUM(NRE_API_MODE_FUNCTION_DRAW, NRE::ApiMode::FunctionDrawMode)
    NRE_CHECK_ENUM(NRE_ERROR_POLICY_IGNORE, NRE::RenderErrorPolicy::Ignore)
    NRE_CHECK_ENUM(NRE_ERROR_POLICY_RETURN, NRE::RenderErrorPolicy::Return)
    NRE_CHECK_ENUM(NRE_ERROR_POLICY_PANIC, NRE::RenderErrorPolicy::Panic)
    NRE_CHECK_ENUM(NRE_MOUSE_BUTTON_LEFT, NRE::MouseButton::Left)
    NRE_CHECK_ENUM(NRE_MOUSE_BUTTON_RIGHT, NRE::MouseButton::Right)
    NRE_CHECK_ENUM(NRE_MOUSE_BUTTON_MIDDLE, NRE::MouseButton::Middle)
    NRE_CHECK_ENUM(NRE_CURSOR_NORMAL, NRE::CursorMode::Normal)
    NRE_CHECK_ENUM(NRE_CURSOR_HIDDEN, NRE::CursorMode::Hidden)
    NRE_CHECK_ENUM(NRE_CURSOR_LOCKED, NRE::CursorMode::Locked)
    // Keys: both sides spell out the GLFW codes; check the start of each run of values.
    NRE_CHECK_ENUM(NRE_KEY_SPACE, NRE::Key::Space)
    NRE_CHECK_ENUM(NRE_KEY_9, NRE::Key::Nine)
    NRE_CHECK_ENUM(NRE_KEY_Z, NRE::Key::Z)
    NRE_CHECK_ENUM(NRE_KEY_GRAVE_ACCENT, NRE::Key::GraveAccent)
    NRE_CHECK_ENUM(NRE_KEY_END, NRE::Key::End)
    NRE_CHECK_ENUM(NRE_KEY_PAUSE, NRE::Key::Pause)
    NRE_CHECK_ENUM(NRE_KEY_F12, NRE::Key::F12)
    NRE_CHECK_ENUM(NRE_KEY_KP_EQUAL, NRE::Key::KeypadEqual)
    NRE_CHECK_ENUM(NRE_KEY_MENU, NRE::Key::Menu)
#undef NRE_CHECK_ENUM

    NRE::ShaderType toCpp(NreShaderType v) { return static_cast<NRE::ShaderType>(v); }
    NRE::MouseButton toCpp(NreMouseButton v) { return static_cast<NRE::MouseButton>(v); }
    NRE::CursorMode toCpp(NreCursorMode v) { return static_cast<NRE::CursorMode>(v); }
    NRE::Key toCpp(NreKey v) { return static_cast<NRE::Key>(v); }

    // ---------------------------------------------------------------------------------------------
    // Math types
    // ---------------------------------------------------------------------------------------------
    glm::vec3 toCpp(NreVec3 v) { return {v.x, v.y, v.z}; }
    NRE::Transform toCpp(const NreTransform& t) {
        NRE::Transform result;
        result.position = toCpp(t.position);
        result.rotation = glm::quat(t.rotation.w, t.rotation.x, t.rotation.y, t.rotation.z);
        result.scale = toCpp(t.scale);
        return result;
    }

    void store(bool* out, bool v) { *out = v; }
    void store(float* out, float v) { *out = v; }
    void store(NreVec2* out, glm::vec2 v) { *out = {v.x, v.y}; }
    void store(NreVec3* out, glm::vec3 v) { *out = {v.x, v.y, v.z}; }
    void store(NreMat4* out, const glm::mat4& m) { std::memcpy(out->m, glm::value_ptr(m), sizeof(out->m)); }
    void store(NreTransform* out, const NRE::Transform& t) {
        store(&out->position, t.position);
        out->rotation = {t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w};
        store(&out->scale, t.scale);
    }

    // ---------------------------------------------------------------------------------------------
    // Handles: the C index is the C++ index + 1, so the empty C++ handle (index UINT32_MAX) is {0, 0} in C.
    // Unsigned wrap-around does both directions.
    // ---------------------------------------------------------------------------------------------
#define NRE_C_HANDLE(CType, Tag)                                                                          \
    NRE::Handel<NRE::Tag> toCpp(CType h) { return {h.index - 1u, h.generation}; }                        \
    void store(CType* out, NRE::Handel<NRE::Tag> h) { *out = {h.index + 1u, h.generation}; }
    NRE_C_HANDLE(NreShader, Shader)
    NRE_C_HANDLE(NrePipeline, Pipeline)
    NRE_C_HANDLE(NreModel, Model)
    NRE_C_HANDLE(NreInstance, Instance)
    NRE_C_HANDLE(NreLight, Light)
    NRE_C_HANDLE(NreScene, Scene)
    NRE_C_HANDLE(NreTexture, Texture)
    NRE_C_HANDLE(NreMaterial, Material)
    NRE_C_HANDLE(NreCubemap, Cubemap)
#undef NRE_C_HANDLE
    static_assert(NRE::Handel<NRE::Model>{}.index + 1u == 0u, "the empty C++ handle must map to {0, 0}");
    // nreCreateMeshModel passes the C vertex array straight through.
    static_assert(sizeof(NreVertex) == sizeof(NRE::Vertex) && offsetof(NreVertex, normal) == offsetof(NRE::Vertex, normal)
                  && offsetof(NreVertex, uv) == offsetof(NRE::Vertex, uv), "NreVertex must match NRE::Vertex");

    // ---------------------------------------------------------------------------------------------
    // Results
    // ---------------------------------------------------------------------------------------------
    thread_local std::string lastError;

    NreErrorCode fail(NRE::Error& error) {
        lastError = std::move(error.error);
        return toC(error.code);
    }

    NreErrorCode ret(NRE::Result<void>&& result) {
        return result ? NRE_OK : fail(result.error());
    }

    template <class T, class Out>
    NreErrorCode ret(NRE::Result<T>&& result, Out* out) {
        if (!result) return fail(result.error());
        store(out, *result);
        return NRE_OK;
    }

    // Runs one wrapper body. C has no exceptions, and one unwinding into a C or FFI caller is undefined,
    // so they stop here (e.g. std::bad_alloc, or a NULL string turned into a std::string).
    template <class F>
    NreErrorCode guard(F&& body) noexcept {
        try {
            return body();
        } catch (const std::exception& e) {
            lastError = std::string("C++ exception inside the renderer: ") + e.what();
        } catch (...) {
            lastError = "unknown C++ exception inside the renderer";
        }
        return NRE_ERROR_UNKNOWN;
    }
}

// The wrappers. Each is `return guard([&] { return ret(NRE::xxx(converted args), out); });`.
#define NRE_C_CALL(...) return guard([&] { return ret(__VA_ARGS__); })

extern "C" {

// -------------------------------------------------------------------------------------------------
// Errors and version
// -------------------------------------------------------------------------------------------------
NRE_C_EXPORT const char* nreGetLastErrorMessage(void) {
    return lastError.c_str();
}

NRE_C_EXPORT const char* nreErrorCodeToString(NreErrorCode code) {
    if (code == NRE_OK) return "Ok";
    // toString returns views of string literals, so data() is null-terminated.
    return NRE::toString(toCpp(code)).data();
}

NRE_C_EXPORT void nreGetVersion(uint32_t* major, uint32_t* minor, uint32_t* patch) {
    if (major) *major = NRE_VERSION_MAJOR;
    if (minor) *minor = NRE_VERSION_MINOR;
    if (patch) *patch = NRE_VERSION_PATCH;
}

// -------------------------------------------------------------------------------------------------
// Setup and frames
// -------------------------------------------------------------------------------------------------
NRE_C_EXPORT NreErrorCode nreSetRenderConfig(NreRenderMode mode, NreApiMode apiMode, NreRenderErrorPolicy errorPolicy,
                                             bool manualFramePresentationAndSubmission,
                                             int windowWidth, int windowHeight, const char* windowTitle,
                                             bool resizable, bool showFpsInTitle) {
    return guard([&] {
        NRE::RenderConfig config{
            .mode = static_cast<NRE::RenderMode>(mode),
            .apiMode = static_cast<NRE::ApiMode>(apiMode),
            .errorPolicy = static_cast<NRE::RenderErrorPolicy>(errorPolicy),
            .manualFramePresentationAndSubmission = manualFramePresentationAndSubmission,
            .windowConfig = {
                .width = windowWidth,
                .height = windowHeight,
                .title = windowTitle,
                .resizable = resizable,
                .showFpsInTitle = showFpsInTitle,
            },
        };
        return ret(NRE::setRenderConfig(std::move(config)));
    });
}
NRE_C_EXPORT NreErrorCode nreInit(void) { NRE_C_CALL(NRE::init()); }
NRE_C_EXPORT NreErrorCode nreShutdown(void) { NRE_C_CALL(NRE::shutdown()); }
NRE_C_EXPORT NreErrorCode nreRenderFrame(void) { NRE_C_CALL(NRE::renderFrame()); }
NRE_C_EXPORT NreErrorCode nreEndFrame(void) { NRE_C_CALL(NRE::endFrame()); }
NRE_C_EXPORT NreErrorCode nrePresentFrame(void) { NRE_C_CALL(NRE::presentFrame()); }
NRE_C_EXPORT NreErrorCode nreSubmitFrame(void) { NRE_C_CALL(NRE::submitFrame()); }
NRE_C_EXPORT NreErrorCode nreWindowShouldClose(bool* shouldClose) { NRE_C_CALL(NRE::windowShouldClose(), shouldClose); }
NRE_C_EXPORT NreErrorCode nrePollEvents(void) { NRE_C_CALL(NRE::pollEvents()); }

// -------------------------------------------------------------------------------------------------
// Shaders and pipelines
// -------------------------------------------------------------------------------------------------
NRE_C_EXPORT NreErrorCode nreCreateShader(const char* shaderPath, NreShaderType type, NreShader* shader) {
    NRE_C_CALL(NRE::createShader(shaderPath, toCpp(type)), shader);
}
NRE_C_EXPORT NreErrorCode nreDestroyShader(NreShader shader) { NRE_C_CALL(NRE::destroyShader(toCpp(shader))); }
NRE_C_EXPORT NreErrorCode nreCreatePipeline(NreShader vertexShader, NreShader fragmentShader, NrePipeline* pipeline) {
    NRE_C_CALL(NRE::createPipeline(toCpp(vertexShader), toCpp(fragmentShader)), pipeline);
}
NRE_C_EXPORT NreErrorCode nreDestroyPipeline(NrePipeline pipeline) { NRE_C_CALL(NRE::destroyPipeline(toCpp(pipeline))); }
NRE_C_EXPORT NreErrorCode nreBindPipeline(NrePipeline pipeline) { NRE_C_CALL(NRE::bindPipeline(toCpp(pipeline))); }

// -------------------------------------------------------------------------------------------------
// Models and instances
// -------------------------------------------------------------------------------------------------
NRE_C_EXPORT NreErrorCode nreCreateModel(const char* modelPath, NreModel* model) {
    NRE_C_CALL(NRE::createModel(modelPath), model);
}
NRE_C_EXPORT NreErrorCode nreCreateMeshModel(const NreVertex* vertices, uint32_t vertexCount, const uint32_t* indices,
                                             uint32_t indexCount, NreMaterial material, NreModel* model) {
    NRE_C_CALL(NRE::createMeshModel({reinterpret_cast<const NRE::Vertex*>(vertices), vertexCount},
                                    {indices, indexCount}, toCpp(material)), model);
}
NRE_C_EXPORT NreErrorCode nreDestroyModel(NreModel model) { NRE_C_CALL(NRE::destroyModel(toCpp(model))); }
NRE_C_EXPORT NreErrorCode nreCreateInstance(NreModel model, NreInstance* instance) {
    NRE_C_CALL(NRE::createInstance(toCpp(model)), instance);
}
NRE_C_EXPORT NreErrorCode nreDestroyInstance(NreInstance instance) { NRE_C_CALL(NRE::destroyInstance(toCpp(instance))); }
NRE_C_EXPORT NreErrorCode nreSetInstanceTransform(NreInstance instance, const NreTransform* transform) {
    NRE_C_CALL(NRE::setInstanceTransform(toCpp(instance), toCpp(*transform)));
}
NRE_C_EXPORT NreErrorCode nreGetInstanceTransform(NreInstance instance, NreTransform* transform) {
    NRE_C_CALL(NRE::getInstanceTransform(toCpp(instance)), transform);
}
NRE_C_EXPORT NreErrorCode nreGetInstanceMatrix(NreInstance instance, NreMat4* matrix) {
    NRE_C_CALL(NRE::getInstanceMatrix(toCpp(instance)), matrix);
}
NRE_C_EXPORT NreErrorCode nreTranslateInstance(NreInstance instance, NreVec3 offset) {
    NRE_C_CALL(NRE::translateInstance(toCpp(instance), toCpp(offset)));
}
NRE_C_EXPORT NreErrorCode nreRotateInstance(NreInstance instance, float angle, NreVec3 axis) {
    NRE_C_CALL(NRE::rotateInstance(toCpp(instance), angle, toCpp(axis)));
}
NRE_C_EXPORT NreErrorCode nreScaleInstance(NreInstance instance, NreVec3 factor) {
    NRE_C_CALL(NRE::scaleInstance(toCpp(instance), toCpp(factor)));
}
NRE_C_EXPORT NreErrorCode nreSetVisibility(NreInstance instance, bool visible) {
    NRE_C_CALL(NRE::setVisibility(toCpp(instance), visible));
}
NRE_C_EXPORT NreErrorCode nreDrawModel(NreModel model, const NreTransform* transform) {
    NRE_C_CALL(NRE::drawModel(toCpp(model), transform ? toCpp(*transform) : NRE::Transform{}));
}
NRE_C_EXPORT NreErrorCode nreDrawInstance(NreInstance instance) { NRE_C_CALL(NRE::drawInstance(toCpp(instance))); }

// -------------------------------------------------------------------------------------------------
// Scenes
// -------------------------------------------------------------------------------------------------
NRE_C_EXPORT NreErrorCode nreCreateScene(NreScene* scene) { NRE_C_CALL(NRE::createScene(), scene); }
NRE_C_EXPORT NreErrorCode nreDestroyScene(NreScene scene) { NRE_C_CALL(NRE::destroyScene(toCpp(scene))); }
NRE_C_EXPORT NreErrorCode nreBindScene(NreScene scene) { NRE_C_CALL(NRE::bindScene(toCpp(scene))); }
NRE_C_EXPORT NreErrorCode nreGetBoundScene(NreScene* scene) { NRE_C_CALL(NRE::getBoundScene(), scene); }
NRE_C_EXPORT NreErrorCode nreAddInstanceToScene(NreInstance instance) {
    NRE_C_CALL(NRE::addInstanceToScene(toCpp(instance)));
}
NRE_C_EXPORT NreErrorCode nreRemoveInstanceFromScene(NreInstance instance) {
    NRE_C_CALL(NRE::removeInstanceFromScene(toCpp(instance)));
}

// -------------------------------------------------------------------------------------------------
// Textures and materials
// -------------------------------------------------------------------------------------------------
NRE_C_EXPORT NreErrorCode nreCreateTexture(const char* path, NreTexture* texture) {
    NRE_C_CALL(NRE::createTexture(path), texture);
}
NRE_C_EXPORT NreErrorCode nreDestroyTexture(NreTexture texture) { NRE_C_CALL(NRE::destroyTexture(toCpp(texture))); }
NRE_C_EXPORT NreErrorCode nreCreateMaterial(NreTexture baseTexture, NreTexture metallicTexture, NreTexture normalTexture,
                                            NreMaterial* material) {
    NRE_C_CALL(NRE::createMaterial(toCpp(baseTexture), toCpp(metallicTexture), toCpp(normalTexture)), material);
}
NRE_C_EXPORT NreErrorCode nreDestroyMaterial(NreMaterial material) { NRE_C_CALL(NRE::destroyMaterial(toCpp(material))); }
NRE_C_EXPORT NreErrorCode nreSetMaterialBaseTexture(NreMaterial material, NreTexture texture) {
    NRE_C_CALL(NRE::setMaterialBaseTexture(toCpp(material), toCpp(texture)));
}
NRE_C_EXPORT NreErrorCode nreGetModelMaterials(NreModel model, uint32_t* count, NreMaterial* materials) {
    return guard([&] {
        auto result = NRE::getModelMaterials(toCpp(model));
        if (!result) return fail(result.error());
        uint32_t total = static_cast<uint32_t>(result->size());
        if (!materials) {
            *count = total;
            return NRE_OK;
        }
        uint32_t written = *count < total ? *count : total;
        for (uint32_t i = 0; i < written; ++i) store(&materials[i], (*result)[i]);
        *count = written;
        return NRE_OK;
    });
}
NRE_C_EXPORT NreErrorCode nreSetModelMaterial(NreModel model, NreMaterial material) {
    NRE_C_CALL(NRE::setModelMaterial(toCpp(model), toCpp(material)));
}
NRE_C_EXPORT NreErrorCode nreReplaceModelMaterial(NreModel model, uint32_t index, NreMaterial material) {
    NRE_C_CALL(NRE::replaceModelMaterial(toCpp(model), index, toCpp(material)));
}

// -------------------------------------------------------------------------------------------------
// Lights and the sun
// -------------------------------------------------------------------------------------------------
NRE_C_EXPORT NreErrorCode nreCreateDirectionalLight(NreVec3 direction, NreVec3 color, float intensity, NreLight* light) {
    NRE_C_CALL(NRE::createDirectionalLight(toCpp(direction), toCpp(color), intensity), light);
}
NRE_C_EXPORT NreErrorCode nreCreatePointLight(NreVec3 position, NreVec3 color, float intensity, NreLight* light) {
    NRE_C_CALL(NRE::createPointLight(toCpp(position), toCpp(color), intensity), light);
}
NRE_C_EXPORT NreErrorCode nreDestroyLight(NreLight light) { NRE_C_CALL(NRE::destroyLight(toCpp(light))); }
NRE_C_EXPORT NreErrorCode nreAddLightToScene(NreLight light) { NRE_C_CALL(NRE::addLightToScene(toCpp(light))); }
NRE_C_EXPORT NreErrorCode nreRemoveLightFromScene(NreLight light) { NRE_C_CALL(NRE::removeLightFromScene(toCpp(light))); }
NRE_C_EXPORT NreErrorCode nreSetLightPosition(NreLight light, NreVec3 position) {
    NRE_C_CALL(NRE::setLightPosition(toCpp(light), toCpp(position)));
}
NRE_C_EXPORT NreErrorCode nreSetLightDirection(NreLight light, NreVec3 direction) {
    NRE_C_CALL(NRE::setLightDirection(toCpp(light), toCpp(direction)));
}
NRE_C_EXPORT NreErrorCode nreSetLightColor(NreLight light, NreVec3 color, float intensity) {
    NRE_C_CALL(NRE::setLightColor(toCpp(light), toCpp(color), intensity));
}
NRE_C_EXPORT NreErrorCode nreSetSun(NreVec3 direction, NreVec3 color, float intensity) {
    NRE_C_CALL(NRE::setSun(toCpp(direction), toCpp(color), intensity));
}
NRE_C_EXPORT NreErrorCode nreSetSunDirection(NreVec3 direction) { NRE_C_CALL(NRE::setSunDirection(toCpp(direction))); }
NRE_C_EXPORT NreErrorCode nreGetSunDirection(NreVec3* direction) { NRE_C_CALL(NRE::getSunDirection(), direction); }

// -------------------------------------------------------------------------------------------------
// Cubemaps and the skybox
// -------------------------------------------------------------------------------------------------
NRE_C_EXPORT NreErrorCode nreCreateCubemap(const char* right, const char* left, const char* top,
                                           const char* bottom, const char* front, const char* back,
                                           NreCubemap* cubemap) {
    NRE_C_CALL(NRE::createCubemap(right, left, top, bottom, front, back), cubemap);
}
NRE_C_EXPORT NreErrorCode nreDestroyCubemap(NreCubemap cubemap) { NRE_C_CALL(NRE::destroyCubemap(toCpp(cubemap))); }
NRE_C_EXPORT NreErrorCode nreSetSkybox(NreCubemap cubemap) { NRE_C_CALL(NRE::setSkybox(toCpp(cubemap))); }

// -------------------------------------------------------------------------------------------------
// Camera
// -------------------------------------------------------------------------------------------------
NRE_C_EXPORT NreErrorCode nreSetCameraPosition(NreVec3 position) { NRE_C_CALL(NRE::setCameraPosition(toCpp(position))); }
NRE_C_EXPORT NreErrorCode nreGetCameraPosition(NreVec3* position) { NRE_C_CALL(NRE::getCameraPosition(), position); }
NRE_C_EXPORT NreErrorCode nreMoveCamera(NreVec3 offset) { NRE_C_CALL(NRE::moveCamera(toCpp(offset))); }
NRE_C_EXPORT NreErrorCode nreMoveCameraRelative(NreVec3 offset) { NRE_C_CALL(NRE::moveCameraRelative(toCpp(offset))); }
NRE_C_EXPORT NreErrorCode nreSetCameraRotation(float yaw, float pitch) { NRE_C_CALL(NRE::setCameraRotation(yaw, pitch)); }
NRE_C_EXPORT NreErrorCode nreRotateCamera(float yawDelta, float pitchDelta) {
    NRE_C_CALL(NRE::rotateCamera(yawDelta, pitchDelta));
}
NRE_C_EXPORT NreErrorCode nreCameraLookAt(NreVec3 target) { NRE_C_CALL(NRE::cameraLookAt(toCpp(target))); }
NRE_C_EXPORT NreErrorCode nreGetCameraForward(NreVec3* forward) { NRE_C_CALL(NRE::getCameraForward(), forward); }
NRE_C_EXPORT NreErrorCode nreGetViewMatrix(NreMat4* view) { NRE_C_CALL(NRE::getViewMatrix(), view); }

// -------------------------------------------------------------------------------------------------
// Timing
// -------------------------------------------------------------------------------------------------
NRE_C_EXPORT NreErrorCode nreGetFPS(float* fps) { NRE_C_CALL(NRE::getFPS(), fps); }
NRE_C_EXPORT NreErrorCode nreGetDeltaTime(float* deltaTime) { NRE_C_CALL(NRE::getDeltaTime(), deltaTime); }

// -------------------------------------------------------------------------------------------------
// Input
// -------------------------------------------------------------------------------------------------
NRE_C_EXPORT NreErrorCode nreIsMouseButtonDown(NreMouseButton button, bool* down) {
    NRE_C_CALL(NRE::isMouseButtonDown(toCpp(button)), down);
}
NRE_C_EXPORT NreErrorCode nreIsMouseButtonPressed(NreMouseButton button, bool* pressed) {
    NRE_C_CALL(NRE::isMouseButtonPressed(toCpp(button)), pressed);
}
NRE_C_EXPORT NreErrorCode nreIsMouseButtonReleased(NreMouseButton button, bool* released) {
    NRE_C_CALL(NRE::isMouseButtonReleased(toCpp(button)), released);
}
NRE_C_EXPORT NreErrorCode nreGetMouseDelta(NreVec2* delta) { NRE_C_CALL(NRE::getMouseDelta(), delta); }
NRE_C_EXPORT NreErrorCode nreGetScrollDelta(float* delta) { NRE_C_CALL(NRE::getScrollDelta(), delta); }
NRE_C_EXPORT NreErrorCode nreGetMousePosition(NreVec2* position) { NRE_C_CALL(NRE::getMousePosition(), position); }
NRE_C_EXPORT NreErrorCode nreSetCursorMode(NreCursorMode mode) { NRE_C_CALL(NRE::setCursorMode(toCpp(mode))); }
NRE_C_EXPORT NreErrorCode nreIsKeyDown(NreKey key, bool* down) { NRE_C_CALL(NRE::isKeyDown(toCpp(key)), down); }
NRE_C_EXPORT NreErrorCode nreIsKeyPressed(NreKey key, bool* pressed) { NRE_C_CALL(NRE::isKeyPressed(toCpp(key)), pressed); }
NRE_C_EXPORT NreErrorCode nreIsKeyReleased(NreKey key, bool* released) {
    NRE_C_CALL(NRE::isKeyReleased(toCpp(key)), released);
}

} // extern "C"
