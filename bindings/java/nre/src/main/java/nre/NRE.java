package nre;

import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Optional;

import static java.lang.foreign.ValueLayout.JAVA_BOOLEAN;
import static java.lang.foreign.ValueLayout.JAVA_FLOAT;
import static java.lang.foreign.ValueLayout.JAVA_INT;
import static nre.Native.*;

/**
 * The NRE renderer, over its C API (nre.h) with the Foreign Function &amp; Memory API. One static method per nre.h
 * function, named like the C++ API in renderer.h ({@code nreCreateModel} is {@link #createModel}); see renderer.h
 * for what each one does.
 *
 * <p>Errors: a failed call throws {@link NreException} with the {@link ErrorCode} and the library's message.
 * Out-parameters are return values. Default arguments of the C++ API are overloads here.
 *
 * <p>Threads: call NRE from the thread that called {@link #init} (GLFW wants the main thread; on macOS run the
 * JVM with {@code -XstartOnFirstThread}). Don't make NRE calls from virtual threads: the error message is kept
 * per OS thread, and a virtual thread can move to another carrier thread between the failing call and reading
 * it.
 *
 * <p>Handles ({@link Model}, {@link Instance}, ...) are plain values; nothing is freed by the garbage collector,
 * destroy them with the matching {@code destroyXxx} like in C++.
 *
 * <p>Loading: the first use loads {@code libnre_c} from {@code $NRE_LIB_DIR} if that is set, otherwise by name
 * through the system's library search, and checks that its version is 0.3.x. The JVM needs
 * {@code --enable-native-access=ALL-UNNAMED} (or the module name) to allow the native calls without warnings.
 */
public final class NRE {
    private NRE() {}

    // ---------------------------------------------------------------------------------------------
    // Errors and version
    // ---------------------------------------------------------------------------------------------

    /** The message of the most recent failed call on this thread ("" if none failed yet). */
    public static String getLastErrorMessage() {
        try {
            return string((MemorySegment) GET_LAST_ERROR_MESSAGE.invokeExact());
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    /** "Ok", "FileNotFound", ... */
    public static String errorCodeToString(ErrorCode code) {
        try {
            return string((MemorySegment) ERROR_CODE_TO_STRING.invokeExact(code.value()));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    /** The loaded library's version (checked to be 0.3.x when it was loaded). */
    public static Version getVersion() {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment major = a.allocate(JAVA_INT), minor = a.allocate(JAVA_INT), patch = a.allocate(JAVA_INT);
            GET_VERSION.invokeExact(major, minor, patch);
            return new Version(major.get(JAVA_INT, 0), minor.get(JAVA_INT, 0), patch.get(JAVA_INT, 0));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    // ---------------------------------------------------------------------------------------------
    // Setup and frames
    // ---------------------------------------------------------------------------------------------

    /**
     * Must come before {@link #init}. See renderer.h's RenderConfig/WindowConfig.
     *
     * @param manualFramePresentationAndSubmission if true, renderFrame/endFrame don't submit and present:
     *        call {@link #submitFrame} and {@link #presentFrame} yourself
     */
    public static void setRenderConfig(int width, int height, String title, RenderMode mode, ApiMode apiMode,
                                       RenderErrorPolicy errorPolicy, boolean manualFramePresentationAndSubmission,
                                       boolean resizable, boolean showFpsInTitle) {
        try (Arena a = Arena.ofConfined()) {
            check((int) SET_RENDER_CONFIG.invokeExact(mode.value(), apiMode.value(), errorPolicy.value(),
                    manualFramePresentationAndSubmission, width, height, a.allocateFrom(title), resizable,
                    showFpsInTitle));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    /** No manual presentation, resizable, FPS in the title. */
    public static void setRenderConfig(int width, int height, String title, RenderMode mode, ApiMode apiMode,
                                       RenderErrorPolicy errorPolicy) {
        setRenderConfig(width, height, title, mode, apiMode, errorPolicy, false, true, true);
    }

    /** Error policy RETURN, no manual presentation, resizable, FPS in the title. */
    public static void setRenderConfig(int width, int height, String title, RenderMode mode, ApiMode apiMode) {
        setRenderConfig(width, height, title, mode, apiMode, RenderErrorPolicy.RETURN);
    }

    /** 3D, SceneMode, error policy RETURN, no manual presentation, resizable, FPS in the title. */
    public static void setRenderConfig(int width, int height, String title) {
        setRenderConfig(width, height, title, RenderMode.RENDER_3D, ApiMode.SCENE);
    }

    /** Opens the window and sets up Vulkan. Call NRE from this thread from now on. */
    public static void init() { call0(INIT_CALL); }

    public static void shutdown() { call0(SHUTDOWN_CALL); }

    /**
     * SceneMode: draws the bound scene and (unless manual presentation) submits and presents it.
     * FunctionDrawMode: starts the frame; then drawModel/drawInstance, then {@link #endFrame}.
     */
    public static void renderFrame() { call0(RENDER_FRAME_CALL); }

    /** FunctionDrawMode only: ends the frame renderFrame started and (unless manual presentation) presents it. */
    public static void endFrame() { call0(END_FRAME_CALL); }

    public static void presentFrame() { call0(PRESENT_FRAME_CALL); }

    public static void submitFrame() { call0(SUBMIT_FRAME_CALL); }

    public static boolean windowShouldClose() { return outBool(WINDOW_SHOULD_CLOSE_CALL); }

    public static void pollEvents() { call0(POLL_EVENTS_CALL); }

    // ---------------------------------------------------------------------------------------------
    // Shaders and pipelines
    // ---------------------------------------------------------------------------------------------

    /** Loads a compiled SPIR-V file. */
    public static Shader createShader(String shaderPath, ShaderType type) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment out = a.allocate(HANDLE);
            check((int) CREATE_SHADER.invokeExact(a.allocateFrom(shaderPath), type.value(), out));
            return new Shader(index(out, 0), generation(out, 0));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    public static void destroyShader(Shader shader) { callH(DESTROY_SHADER_CALL, shader); }

    public static Pipeline createPipeline(Shader vertexShader, Shader fragmentShader) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment out = a.allocate(HANDLE);
            check((int) CREATE_PIPELINE.invokeExact(handle(a, vertexShader), handle(a, fragmentShader), out));
            return new Pipeline(index(out, 0), generation(out, 0));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    public static void destroyPipeline(Pipeline pipeline) { callH(DESTROY_PIPELINE_CALL, pipeline); }

    public static void bindPipeline(Pipeline pipeline) { callH(BIND_PIPELINE_CALL, pipeline); }

    // ---------------------------------------------------------------------------------------------
    // Models and instances
    // ---------------------------------------------------------------------------------------------

    /** Loads a model file (glTF, OBJ, ...). A model isn't drawn by itself: draw it through instances. */
    public static Model createModel(String modelPath) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment out = a.allocate(HANDLE);
            check((int) CREATE_MODEL.invokeExact(a.allocateFrom(modelPath), out));
            return new Model(index(out, 0), generation(out, 0));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    /**
     * A model from your own triangles: every three indices (into vertices) are one triangle, counter-clockwise
     * seen from the front. Vertices with a {@link Vec3#ZERO} normal get one computed from their triangles. The
     * arrays are copied (into one native buffer each); {@link Material#NONE} uses the default material. Draw it
     * through instances like a loaded model.
     */
    public static Model createMeshModel(List<Vertex> vertices, int[] indices, Material material) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment out = a.allocate(HANDLE);
            MemorySegment idx = indices.length == 0 ? MemorySegment.NULL : a.allocateFrom(JAVA_INT, indices);
            check((int) CREATE_MESH_MODEL.invokeExact(Native.vertices(a, vertices), vertices.size(), idx,
                    indices.length, handle(a, material), out));
            return new Model(index(out, 0), generation(out, 0));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    /** {@link #createMeshModel(List, int[], Material)} with the default material. */
    public static Model createMeshModel(List<Vertex> vertices, int[] indices) {
        return createMeshModel(vertices, indices, Material.NONE);
    }

    /** {@link #createMeshModel(List, int[], Material)} from an array. */
    public static Model createMeshModel(Vertex[] vertices, int[] indices, Material material) {
        return createMeshModel(Arrays.asList(vertices), indices, material);
    }

    /** {@link #createMeshModel(List, int[], Material)} from an array, with the default material. */
    public static Model createMeshModel(Vertex[] vertices, int[] indices) {
        return createMeshModel(Arrays.asList(vertices), indices, Material.NONE);
    }

    /** Instances of a destroyed model stay valid but draw nothing. */
    public static void destroyModel(Model model) { callH(DESTROY_MODEL_CALL, model); }

    public static Instance createInstance(Model model) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment out = a.allocate(HANDLE);
            check((int) CREATE_INSTANCE.invokeExact(handle(a, model), out));
            return new Instance(index(out, 0), generation(out, 0));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    /** Also removes it from every scene it's in. */
    public static void destroyInstance(Instance instance) { callH(DESTROY_INSTANCE_CALL, instance); }

    public static void setInstanceTransform(Instance instance, Transform transform) {
        try (Arena a = Arena.ofConfined()) {
            check((int) SET_INSTANCE_TRANSFORM.invokeExact(handle(a, instance), Native.transform(a, transform)));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    public static Transform getInstanceTransform(Instance instance) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment out = a.allocate(TRANSFORM);
            check((int) GET_INSTANCE_TRANSFORM.invokeExact(handle(a, instance), out));
            return readTransform(out);
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    public static Mat4 getInstanceMatrix(Instance instance) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment out = a.allocate(MAT4);
            check((int) GET_INSTANCE_MATRIX.invokeExact(handle(a, instance), out));
            return readMat4(out);
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    public static void translateInstance(Instance instance, Vec3 offset) {
        callHV(TRANSLATE_INSTANCE_CALL, instance, offset);
    }

    /** Angle is in radians. */
    public static void rotateInstance(Instance instance, float angle, Vec3 axis) {
        try (Arena a = Arena.ofConfined()) {
            check((int) ROTATE_INSTANCE.invokeExact(handle(a, instance), angle, vec3(a, axis)));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    public static void scaleInstance(Instance instance, Vec3 factor) { callHV(SCALE_INSTANCE_CALL, instance, factor); }

    /** A hidden instance stays in its scenes but isn't drawn. */
    public static void setVisibility(Instance instance, boolean visible) {
        try (Arena a = Arena.ofConfined()) {
            check((int) SET_VISIBILITY.invokeExact(handle(a, instance), visible));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    /** Whether the instance casts sun shadows (it still receives them). Default true, e.g. false for grass. */
    public static void setCastsShadows(Instance instance, boolean castsShadows) {
        try (Arena a = Arena.ofConfined()) {
            check((int) SET_CASTS_SHADOWS.invokeExact(handle(a, instance), castsShadows));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    /** FunctionDrawMode only, between renderFrame and endFrame: draws the model once with this transform. */
    public static void drawModel(Model model, Transform transform) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment t = transform == null ? MemorySegment.NULL : Native.transform(a, transform);
            check((int) DRAW_MODEL.invokeExact(handle(a, model), t));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    /** {@link #drawModel(Model, Transform)} with the identity transform. */
    public static void drawModel(Model model) { drawModel(model, null); }

    /** Uses the instance's transform. A hidden instance isn't drawn. */
    public static void drawInstance(Instance instance) { callH(DRAW_INSTANCE_CALL, instance); }

    // ---------------------------------------------------------------------------------------------
    // Scenes
    // ---------------------------------------------------------------------------------------------

    public static Scene createScene() {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment out = a.allocate(HANDLE);
            check((int) CREATE_SCENE.invokeExact(out));
            return new Scene(index(out, 0), generation(out, 0));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    public static void destroyScene(Scene scene) { callH(DESTROY_SCENE_CALL, scene); }

    public static void bindScene(Scene scene) { callH(BIND_SCENE_CALL, scene); }

    public static Scene getBoundScene() {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment out = a.allocate(HANDLE);
            check((int) GET_BOUND_SCENE.invokeExact(out));
            return new Scene(index(out, 0), generation(out, 0));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    /** Adds to the bound scene. Adding an instance that's already in it does nothing. */
    public static void addInstanceToScene(Instance instance) { callH(ADD_INSTANCE_TO_SCENE_CALL, instance); }

    public static void removeInstanceFromScene(Instance instance) { callH(REMOVE_INSTANCE_FROM_SCENE_CALL, instance); }

    // ---------------------------------------------------------------------------------------------
    // Textures and materials. Texture.NONE uses the default texture.
    // ---------------------------------------------------------------------------------------------

    /**
     * Loads an image file (PNG, JPEG, ...). srgb: true for colour images (base colour); false for data maps,
     * which hold plain numbers: normal maps and metallic-roughness maps.
     */
    public static Texture createTexture(String path, boolean srgb) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment out = a.allocate(HANDLE);
            check((int) CREATE_TEXTURE.invokeExact(a.allocateFrom(path), srgb, out));
            return new Texture(index(out, 0), generation(out, 0));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    /** Loads an image file as a colour (sRGB) texture; use {@code createTexture(path, false)} for data maps. */
    public static Texture createTexture(String path) { return createTexture(path, true); }

    /** Give the materials that use this texture another one first: they keep pointing at it. */
    public static void destroyTexture(Texture texture) { callH(DESTROY_TEXTURE_CALL, texture); }

    /**
     * Textures as glTF uses them: base colour (sRGB), metallic (roughness in green, metallic in blue) and a
     * tangent-space normal map (+Y up); load the last two with {@code createTexture(path, false)}. Texture.NONE:
     * a white base, not metal with roughness 1, a flat normal.
     */
    public static Material createMaterial(Texture baseTexture, Texture metallicTexture, Texture normalTexture) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment out = a.allocate(HANDLE);
            check((int) CREATE_MATERIAL.invokeExact(handle(a, baseTexture), handle(a, metallicTexture),
                    handle(a, normalTexture), out));
            return new Material(index(out, 0), generation(out, 0));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    public static Material createMaterial(Texture baseTexture, Texture metallicTexture) {
        return createMaterial(baseTexture, metallicTexture, Texture.NONE);
    }

    public static Material createMaterial(Texture baseTexture) {
        return createMaterial(baseTexture, Texture.NONE, Texture.NONE);
    }

    /** A material with the default texture. */
    public static Material createMaterial() { return createMaterial(Texture.NONE, Texture.NONE, Texture.NONE); }

    /** Models still using it fall back to the default material. */
    public static void destroyMaterial(Material material) { callH(DESTROY_MATERIAL_CALL, material); }

    /** Waits for the GPU to finish the frames in flight, so don't call it every frame. */
    public static void setMaterialBaseTexture(Material material, Texture texture) {
        try (Arena a = Arena.ofConfined()) {
            check((int) SET_MATERIAL_BASE_TEXTURE.invokeExact(handle(a, material), handle(a, texture)));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    /** The model's material slots, in the file's material order. Empty for a model without materials. */
    public static List<Material> getModelMaterials(Model model) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment m = handle(a, model);
            MemorySegment count = a.allocate(JAVA_INT);
            check((int) GET_MODEL_MATERIALS.invokeExact(m, count, MemorySegment.NULL));
            int n = count.get(JAVA_INT, 0);
            if (n <= 0) return List.of();
            MemorySegment array = a.allocate(HANDLE, n);
            check((int) GET_MODEL_MATERIALS.invokeExact(m, count, array));
            int written = Math.min(n, count.get(JAVA_INT, 0));
            List<Material> result = new ArrayList<>(written);
            for (int i = 0; i < written; i++) {
                long offset = i * HANDLE.byteSize();
                result.add(new Material(index(array, offset), generation(array, offset)));
            }
            return List.copyOf(result);
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    /** Makes every mesh of the model use material, replacing all of its slots. */
    public static void setModelMaterial(Model model, Material material) {
        try (Arena a = Arena.ofConfined()) {
            check((int) SET_MODEL_MATERIAL.invokeExact(handle(a, model), handle(a, material)));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    /** Replaces slot index (an index into getModelMaterials), so only the meshes using that slot change. */
    public static void replaceModelMaterial(Model model, int index, Material material) {
        try (Arena a = Arena.ofConfined()) {
            check((int) REPLACE_MODEL_MATERIAL.invokeExact(handle(a, model), index, handle(a, material)));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    // ---------------------------------------------------------------------------------------------
    // Lights and the sun. Defaults like renderer.h: color (1,1,1), intensity 3 (directional, sun) or 10 (point).
    // direction is the way the light travels, e.g. (0, -1, 0) shines straight down.
    // ---------------------------------------------------------------------------------------------

    public static Light createDirectionalLight(Vec3 direction, Vec3 color, float intensity) {
        return createLight(CREATE_DIRECTIONAL_LIGHT_CALL, direction, color, intensity);
    }

    public static Light createDirectionalLight(Vec3 direction, Vec3 color) { return createDirectionalLight(direction, color, 3.0f); }

    public static Light createDirectionalLight(Vec3 direction) { return createDirectionalLight(direction, Vec3.ONE, 3.0f); }

    public static Light createPointLight(Vec3 position, Vec3 color, float intensity) {
        return createLight(CREATE_POINT_LIGHT_CALL, position, color, intensity);
    }

    public static Light createPointLight(Vec3 position, Vec3 color) { return createPointLight(position, color, 10.0f); }

    public static Light createPointLight(Vec3 position) { return createPointLight(position, Vec3.ONE, 10.0f); }

    /** Also removes it from every scene it's in. */
    public static void destroyLight(Light light) { callH(DESTROY_LIGHT_CALL, light); }

    /** Adds to the bound scene. Adding a light that's already in it does nothing. */
    public static void addLightToScene(Light light) { callH(ADD_LIGHT_TO_SCENE_CALL, light); }

    public static void removeLightFromScene(Light light) { callH(REMOVE_LIGHT_FROM_SCENE_CALL, light); }

    /** Only used by point lights. */
    public static void setLightPosition(Light light, Vec3 position) { callHV(SET_LIGHT_POSITION_CALL, light, position); }

    /** Only used by directional lights. */
    public static void setLightDirection(Light light, Vec3 direction) { callHV(SET_LIGHT_DIRECTION_CALL, light, direction); }

    public static void setLightColor(Light light, Vec3 color, float intensity) {
        try (Arena a = Arena.ofConfined()) {
            check((int) SET_LIGHT_COLOR.invokeExact(handle(a, light), vec3(a, color), intensity));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    /** The bound scene's sun: one directional light kept apart from the others (see renderer.h). */
    public static void setSun(Vec3 direction, Vec3 color, float intensity) {
        try (Arena a = Arena.ofConfined()) {
            check((int) SET_SUN.invokeExact(vec3(a, direction), vec3(a, color), intensity));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    public static void setSun(Vec3 direction, Vec3 color) { setSun(direction, color, 3.0f); }

    public static void setSun(Vec3 direction) { setSun(direction, Vec3.ONE, 3.0f); }

    /** Keeps the color and intensity, e.g. for a day/night cycle. */
    public static void setSunDirection(Vec3 direction) { callV(SET_SUN_DIRECTION_CALL, direction); }

    public static Vec3 getSunDirection() { return outVec3(GET_SUN_DIRECTION_CALL); }

    // ---------------------------------------------------------------------------------------------
    // Scene lighting, shadows, haze, exposure and sky colour, on the bound scene. Defaults like renderer.h:
    // Small: shadows 20 m, haze off. Landscape: shadows 500 m, haze 8000 m / 300 m. Exposure 1, sky (0.1, 0.3, 0.6).
    // ---------------------------------------------------------------------------------------------

    /** Also resets the shadow distance, haze and sky to the mode's defaults. */
    public static void setSceneLighting(SceneLighting lighting) {
        try {
            check((int) SET_SCENE_LIGHTING.invokeExact(lighting.value()));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    public static SceneLighting getSceneLighting() {
        int value;
        try (Arena a = Arena.ofConfined()) {
            MemorySegment out = a.allocate(JAVA_INT);
            check((int) GET_SCENE_LIGHTING.invokeExact(out));
            value = out.get(JAVA_INT, 0);
        } catch (Throwable t) {
            throw rethrow(t);
        }
        return SceneLighting.fromValue(value);
    }

    /** How far from the camera the sun casts shadows, in metres. 0 turns them off; negative is INVALID_ARGUMENT. */
    public static void setShadowDistance(float meters) { callF(SET_SHADOW_DISTANCE_CALL, meters); }

    /**
     * Distance haze. visibility: metres at which it hides almost everything, 0 = off. height: the haze layer's
     * thickness in metres above y = 0, greater than 0.
     */
    public static void setHaze(float visibility, float height) {
        try {
            check((int) SET_HAZE.invokeExact(visibility, height));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    /** {@link #setHaze(float, float)} with a 300 m haze layer. */
    public static void setHaze(float visibility) { setHaze(visibility, 300.0f); }

    /** Greater than 0; 2 is twice as bright. */
    public static void setExposure(float exposure) { callF(SET_EXPOSURE_CALL, exposure); }

    /** The background without a skybox (Small), linear 0..1. */
    public static void setSkyColor(Vec3 color) { callV(SET_SKY_COLOR_CALL, color); }

    // ---------------------------------------------------------------------------------------------
    // Cubemaps and the skybox
    // ---------------------------------------------------------------------------------------------

    /** Six square images of the same size, in the OpenGL order: +X, -X, +Y, -Y, +Z, -Z. */
    public static Cubemap createCubemap(String right, String left, String top, String bottom, String front,
                                        String back) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment out = a.allocate(HANDLE);
            check((int) CREATE_CUBEMAP.invokeExact(a.allocateFrom(right), a.allocateFrom(left), a.allocateFrom(top),
                    a.allocateFrom(bottom), a.allocateFrom(front), a.allocateFrom(back), out));
            return new Cubemap(index(out, 0), generation(out, 0));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    /** Waits for the GPU to finish the frames in flight. Scenes using it go back to having no skybox. */
    public static void destroyCubemap(Cubemap cubemap) { callH(DESTROY_CUBEMAP_CALL, cubemap); }

    /** The bound scene's sky (3D only). {@link Cubemap#NONE} removes it. */
    public static void setSkybox(Cubemap cubemap) { callH(SET_SKYBOX_CALL, cubemap); }

    // ---------------------------------------------------------------------------------------------
    // Camera. Angles are in radians.
    // ---------------------------------------------------------------------------------------------

    public static void setCameraPosition(Vec3 position) { callV(SET_CAMERA_POSITION_CALL, position); }

    public static Vec3 getCameraPosition() { return outVec3(GET_CAMERA_POSITION_CALL); }

    public static void moveCamera(Vec3 offset) { callV(MOVE_CAMERA_CALL, offset); }

    public static void moveCameraRelative(Vec3 offset) { callV(MOVE_CAMERA_RELATIVE_CALL, offset); }

    public static void setCameraRotation(float yaw, float pitch) {
        try {
            check((int) SET_CAMERA_ROTATION.invokeExact(yaw, pitch));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    public static void rotateCamera(float yawDelta, float pitchDelta) {
        try {
            check((int) ROTATE_CAMERA.invokeExact(yawDelta, pitchDelta));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    public static void cameraLookAt(Vec3 target) { callV(CAMERA_LOOK_AT_CALL, target); }

    public static Vec3 getCameraForward() { return outVec3(GET_CAMERA_FORWARD_CALL); }

    public static Mat4 getViewMatrix() {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment out = a.allocate(MAT4);
            check((int) GET_VIEW_MATRIX.invokeExact(out));
            return readMat4(out);
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    /** Vulkan conventions: clip-space +Y down, reverse-Z with an infinite far plane (see renderer.h). */
    public static Mat4 getProjectionMatrix() {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment out = a.allocate(MAT4);
            check((int) GET_PROJECTION_MATRIX.invokeExact(out));
            return readMat4(out);
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    /**
     * The nearest visible instance of the bound scene under a pixel (in {@link #getMousePosition} units), or
     * empty if there's none. RENDER_3D only.
     */
    public static Optional<RayHit> pickInstance(Vec2 windowPos) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment hit = a.allocate(JAVA_BOOLEAN);
            MemorySegment rayHit = a.allocate(RAY_HIT);
            check((int) PICK_INSTANCE.invokeExact(vec2(a, windowPos), hit, rayHit));
            return hit.get(JAVA_BOOLEAN, 0) ? Optional.of(readRayHit(rayHit)) : Optional.empty();
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    // ---------------------------------------------------------------------------------------------
    // Timing
    // ---------------------------------------------------------------------------------------------

    /** Frames per second, averaged over the last half second. */
    public static float getFPS() { return outFloat(GET_FPS_CALL); }

    /** Seconds between the last two presented frames. */
    public static float getDeltaTime() { return outFloat(GET_DELTA_TIME_CALL); }

    // ---------------------------------------------------------------------------------------------
    // Input. Deltas cover the time between the last two pollEvents calls; pressed/released are true for the
    // one frame after the pollEvents call that saw it happen.
    // ---------------------------------------------------------------------------------------------

    public static boolean isMouseButtonDown(MouseButton button) { return outBool(IS_MOUSE_BUTTON_DOWN_CALL, button.value()); }

    public static boolean isMouseButtonPressed(MouseButton button) { return outBool(IS_MOUSE_BUTTON_PRESSED_CALL, button.value()); }

    public static boolean isMouseButtonReleased(MouseButton button) { return outBool(IS_MOUSE_BUTTON_RELEASED_CALL, button.value()); }

    /** How far the cursor moved, in pixels. +x is right, +y is down. */
    public static Vec2 getMouseDelta() { return outVec2(GET_MOUSE_DELTA_CALL); }

    /** Scroll wheel steps. Positive means scrolling up / away from you. */
    public static float getScrollDelta() { return outFloat(GET_SCROLL_DELTA_CALL); }

    /** Cursor position in pixels from the window's top-left corner, as of the last pollEvents call. */
    public static Vec2 getMousePosition() { return outVec2(GET_MOUSE_POSITION_CALL); }

    public static void setCursorMode(CursorMode mode) {
        try {
            check((int) SET_CURSOR_MODE.invokeExact(mode.value()));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    public static boolean isKeyDown(Key key) { return outBool(IS_KEY_DOWN_CALL, key.value()); }

    public static boolean isKeyPressed(Key key) { return outBool(IS_KEY_PRESSED_CALL, key.value()); }

    public static boolean isKeyReleased(Key key) { return outBool(IS_KEY_RELEASED_CALL, key.value()); }

    // ---------------------------------------------------------------------------------------------
    // Shared call shapes. Each lambda calls invokeExact with the exact static types of its handle.
    // ---------------------------------------------------------------------------------------------

    private interface Call0 { int call() throws Throwable; }
    private interface CallH { int call(MemorySegment handle) throws Throwable; }
    private interface CallV { int call(MemorySegment vec3) throws Throwable; }
    private interface CallHV { int call(MemorySegment handle, MemorySegment vec3) throws Throwable; }
    private interface CallF { int call(float value) throws Throwable; }
    private interface CallOut { int call(MemorySegment out) throws Throwable; }
    private interface CallIntOut { int call(int value, MemorySegment out) throws Throwable; }
    private interface CallLight { int call(MemorySegment v, MemorySegment color, float intensity, MemorySegment out) throws Throwable; }

    private static final Call0 INIT_CALL = () -> (int) INIT.invokeExact();
    private static final Call0 SHUTDOWN_CALL = () -> (int) SHUTDOWN.invokeExact();
    private static final Call0 RENDER_FRAME_CALL = () -> (int) RENDER_FRAME.invokeExact();
    private static final Call0 END_FRAME_CALL = () -> (int) END_FRAME.invokeExact();
    private static final Call0 PRESENT_FRAME_CALL = () -> (int) PRESENT_FRAME.invokeExact();
    private static final Call0 SUBMIT_FRAME_CALL = () -> (int) SUBMIT_FRAME.invokeExact();
    private static final Call0 POLL_EVENTS_CALL = () -> (int) POLL_EVENTS.invokeExact();

    private static final CallH DESTROY_SHADER_CALL = h -> (int) DESTROY_SHADER.invokeExact(h);
    private static final CallH DESTROY_PIPELINE_CALL = h -> (int) DESTROY_PIPELINE.invokeExact(h);
    private static final CallH BIND_PIPELINE_CALL = h -> (int) BIND_PIPELINE.invokeExact(h);
    private static final CallH DESTROY_MODEL_CALL = h -> (int) DESTROY_MODEL.invokeExact(h);
    private static final CallH DESTROY_INSTANCE_CALL = h -> (int) DESTROY_INSTANCE.invokeExact(h);
    private static final CallH DRAW_INSTANCE_CALL = h -> (int) DRAW_INSTANCE.invokeExact(h);
    private static final CallH DESTROY_SCENE_CALL = h -> (int) DESTROY_SCENE.invokeExact(h);
    private static final CallH BIND_SCENE_CALL = h -> (int) BIND_SCENE.invokeExact(h);
    private static final CallH ADD_INSTANCE_TO_SCENE_CALL = h -> (int) ADD_INSTANCE_TO_SCENE.invokeExact(h);
    private static final CallH REMOVE_INSTANCE_FROM_SCENE_CALL = h -> (int) REMOVE_INSTANCE_FROM_SCENE.invokeExact(h);
    private static final CallH DESTROY_TEXTURE_CALL = h -> (int) DESTROY_TEXTURE.invokeExact(h);
    private static final CallH DESTROY_MATERIAL_CALL = h -> (int) DESTROY_MATERIAL.invokeExact(h);
    private static final CallH DESTROY_LIGHT_CALL = h -> (int) DESTROY_LIGHT.invokeExact(h);
    private static final CallH ADD_LIGHT_TO_SCENE_CALL = h -> (int) ADD_LIGHT_TO_SCENE.invokeExact(h);
    private static final CallH REMOVE_LIGHT_FROM_SCENE_CALL = h -> (int) REMOVE_LIGHT_FROM_SCENE.invokeExact(h);
    private static final CallH DESTROY_CUBEMAP_CALL = h -> (int) DESTROY_CUBEMAP.invokeExact(h);
    private static final CallH SET_SKYBOX_CALL = h -> (int) SET_SKYBOX.invokeExact(h);

    private static final CallV SET_SUN_DIRECTION_CALL = v -> (int) SET_SUN_DIRECTION.invokeExact(v);
    private static final CallV SET_CAMERA_POSITION_CALL = v -> (int) SET_CAMERA_POSITION.invokeExact(v);
    private static final CallV MOVE_CAMERA_CALL = v -> (int) MOVE_CAMERA.invokeExact(v);
    private static final CallV MOVE_CAMERA_RELATIVE_CALL = v -> (int) MOVE_CAMERA_RELATIVE.invokeExact(v);
    private static final CallV CAMERA_LOOK_AT_CALL = v -> (int) CAMERA_LOOK_AT.invokeExact(v);

    private static final CallV SET_SKY_COLOR_CALL = v -> (int) SET_SKY_COLOR.invokeExact(v);

    private static final CallF SET_SHADOW_DISTANCE_CALL = f -> (int) SET_SHADOW_DISTANCE.invokeExact(f);
    private static final CallF SET_EXPOSURE_CALL = f -> (int) SET_EXPOSURE.invokeExact(f);

    private static final CallHV TRANSLATE_INSTANCE_CALL = (h, v) -> (int) TRANSLATE_INSTANCE.invokeExact(h, v);
    private static final CallHV SCALE_INSTANCE_CALL = (h, v) -> (int) SCALE_INSTANCE.invokeExact(h, v);
    private static final CallHV SET_LIGHT_POSITION_CALL = (h, v) -> (int) SET_LIGHT_POSITION.invokeExact(h, v);
    private static final CallHV SET_LIGHT_DIRECTION_CALL = (h, v) -> (int) SET_LIGHT_DIRECTION.invokeExact(h, v);

    private static final CallOut WINDOW_SHOULD_CLOSE_CALL = o -> (int) WINDOW_SHOULD_CLOSE.invokeExact(o);
    private static final CallOut GET_SUN_DIRECTION_CALL = o -> (int) GET_SUN_DIRECTION.invokeExact(o);
    private static final CallOut GET_CAMERA_POSITION_CALL = o -> (int) GET_CAMERA_POSITION.invokeExact(o);
    private static final CallOut GET_CAMERA_FORWARD_CALL = o -> (int) GET_CAMERA_FORWARD.invokeExact(o);
    private static final CallOut GET_FPS_CALL = o -> (int) GET_FPS.invokeExact(o);
    private static final CallOut GET_DELTA_TIME_CALL = o -> (int) GET_DELTA_TIME.invokeExact(o);
    private static final CallOut GET_MOUSE_DELTA_CALL = o -> (int) GET_MOUSE_DELTA.invokeExact(o);
    private static final CallOut GET_SCROLL_DELTA_CALL = o -> (int) GET_SCROLL_DELTA.invokeExact(o);
    private static final CallOut GET_MOUSE_POSITION_CALL = o -> (int) GET_MOUSE_POSITION.invokeExact(o);

    private static final CallIntOut IS_MOUSE_BUTTON_DOWN_CALL = (v, o) -> (int) IS_MOUSE_BUTTON_DOWN.invokeExact(v, o);
    private static final CallIntOut IS_MOUSE_BUTTON_PRESSED_CALL = (v, o) -> (int) IS_MOUSE_BUTTON_PRESSED.invokeExact(v, o);
    private static final CallIntOut IS_MOUSE_BUTTON_RELEASED_CALL = (v, o) -> (int) IS_MOUSE_BUTTON_RELEASED.invokeExact(v, o);
    private static final CallIntOut IS_KEY_DOWN_CALL = (v, o) -> (int) IS_KEY_DOWN.invokeExact(v, o);
    private static final CallIntOut IS_KEY_PRESSED_CALL = (v, o) -> (int) IS_KEY_PRESSED.invokeExact(v, o);
    private static final CallIntOut IS_KEY_RELEASED_CALL = (v, o) -> (int) IS_KEY_RELEASED.invokeExact(v, o);

    private static final CallLight CREATE_DIRECTIONAL_LIGHT_CALL =
            (v, c, i, o) -> (int) CREATE_DIRECTIONAL_LIGHT.invokeExact(v, c, i, o);
    private static final CallLight CREATE_POINT_LIGHT_CALL =
            (v, c, i, o) -> (int) CREATE_POINT_LIGHT.invokeExact(v, c, i, o);

    private static void call0(Call0 f) {
        try {
            check(f.call());
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    private static void callH(CallH f, Handle h) {
        try (Arena a = Arena.ofConfined()) {
            check(f.call(handle(a, h)));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    private static void callV(CallV f, Vec3 v) {
        try (Arena a = Arena.ofConfined()) {
            check(f.call(vec3(a, v)));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    private static void callF(CallF f, float value) {
        try {
            check(f.call(value));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    private static void callHV(CallHV f, Handle h, Vec3 v) {
        try (Arena a = Arena.ofConfined()) {
            check(f.call(handle(a, h), vec3(a, v)));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    private static boolean outBool(CallOut f) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment out = a.allocate(JAVA_BOOLEAN);
            check(f.call(out));
            return out.get(JAVA_BOOLEAN, 0);
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    private static boolean outBool(CallIntOut f, int value) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment out = a.allocate(JAVA_BOOLEAN);
            check(f.call(value, out));
            return out.get(JAVA_BOOLEAN, 0);
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    private static float outFloat(CallOut f) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment out = a.allocate(JAVA_FLOAT);
            check(f.call(out));
            return out.get(JAVA_FLOAT, 0);
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    private static Vec2 outVec2(CallOut f) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment out = a.allocate(VEC2);
            check(f.call(out));
            return readVec2(out);
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    private static Vec3 outVec3(CallOut f) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment out = a.allocate(VEC3);
            check(f.call(out));
            return readVec3(out, 0);
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    private static Light createLight(CallLight f, Vec3 v, Vec3 color, float intensity) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment out = a.allocate(HANDLE);
            check(f.call(vec3(a, v), vec3(a, color), intensity, out));
            return new Light(index(out, 0), generation(out, 0));
        } catch (Throwable t) {
            throw rethrow(t);
        }
    }

    /** Throws for a non-NRE_OK code, with the message read right after the call (same thread). */
    private static void check(int code) {
        if (code != 0) throw new NreException(code, getLastErrorMessage());
    }

    private static RuntimeException rethrow(Throwable t) {
        if (t instanceof RuntimeException e) return e;
        if (t instanceof Error e) throw e;
        return new IllegalStateException("NRE: unexpected exception from a native call", t);
    }
}
