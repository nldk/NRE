using System.Runtime.InteropServices;

namespace Nre;

/// <summary>
/// The NRE renderer, over its C API (nre.h). Same functions as renderer.h, in C# style: see renderer.h for what
/// each one does. A failed call throws <see cref="NreException"/>; values come back as return values.
/// The renderer uses GLFW, so call it from the main thread (the one that called <see cref="Init"/>).
/// Handles are plain values: destroy what you create with the matching DestroyXxx.
/// </summary>
public static unsafe class NRE
{
    // ---------------------------------------------------------------------------------------------
    // Errors and version
    // ---------------------------------------------------------------------------------------------

    /// <summary>Maps a C error code to <see cref="ErrorCode"/>; values these bindings don't know become Unknown.</summary>
    internal static ErrorCode ToErrorCode(int code) =>
        code != 0 && Enum.IsDefined((ErrorCode)code) ? (ErrorCode)code : ErrorCode.Unknown;

    private static void Check(int code)
    {
        if (code != 0) throw new NreException(ToErrorCode(code), GetLastErrorMessage());
    }

    private static string Utf8(byte* s) => s == null ? "" : Marshal.PtrToStringUTF8((IntPtr)s) ?? "";

    /// <summary>The message of the most recent failed call on this thread ("" if none failed yet).</summary>
    public static string GetLastErrorMessage() => Utf8(Native.nreGetLastErrorMessage());

    /// <summary>"Ok", "FileNotFound", ...: the library's name for the code. Undefined values are passed as Unknown.</summary>
    public static string ErrorCodeToString(ErrorCode code) =>
        Utf8(Native.nreErrorCodeToString((int)(Enum.IsDefined(code) ? code : ErrorCode.Unknown)));

    /// <summary>The loaded library's version. Loading already checked that major.minor is the one these bindings target.</summary>
    public static (uint Major, uint Minor, uint Patch) GetVersion()
    {
        Native.nreGetVersion(out uint major, out uint minor, out uint patch);
        return (major, minor, patch);
    }

    // ---------------------------------------------------------------------------------------------
    // Setup and frames
    // ---------------------------------------------------------------------------------------------

    /// <summary>Call before <see cref="Init"/>. See renderer.h's RenderConfig / WindowConfig.</summary>
    public static void SetRenderConfig(int width, int height, string title,
                                       RenderMode mode = RenderMode.Render3D,
                                       ApiMode apiMode = ApiMode.SceneMode,
                                       RenderErrorPolicy errorPolicy = RenderErrorPolicy.Return,
                                       bool manualFramePresentationAndSubmission = false,
                                       bool resizable = true,
                                       bool showFpsInTitle = true) =>
        Check(Native.nreSetRenderConfig(mode, apiMode, errorPolicy, manualFramePresentationAndSubmission,
                                        width, height, title, resizable, showFpsInTitle));

    /// <summary>Opens the window and sets up Vulkan. Main thread only (GLFW).</summary>
    public static void Init() => Check(Native.nreInit());
    public static void Shutdown() => Check(Native.nreShutdown());
    /// <summary>SceneMode: draws (and presents) the bound scene. FunctionDrawMode: starts a frame; see renderer.h.</summary>
    public static void RenderFrame() => Check(Native.nreRenderFrame());
    /// <summary>FunctionDrawMode only: ends the frame <see cref="RenderFrame"/> started.</summary>
    public static void EndFrame() => Check(Native.nreEndFrame());
    public static void PresentFrame() => Check(Native.nrePresentFrame());
    public static void SubmitFrame() => Check(Native.nreSubmitFrame());
    /// <summary>Main thread only (GLFW).</summary>
    public static bool WindowShouldClose()
    {
        Check(Native.nreWindowShouldClose(out bool shouldClose));
        return shouldClose;
    }
    /// <summary>Processes window and input events. Main thread only (GLFW).</summary>
    public static void PollEvents() => Check(Native.nrePollEvents());

    // ---------------------------------------------------------------------------------------------
    // Shaders and pipelines
    // ---------------------------------------------------------------------------------------------

    /// <summary>Loads a compiled SPIR-V shader.</summary>
    public static Shader CreateShader(string shaderPath, ShaderType type)
    {
        Check(Native.nreCreateShader(shaderPath, type, out Shader shader));
        return shader;
    }
    public static void DestroyShader(Shader shader) => Check(Native.nreDestroyShader(shader));
    public static Pipeline CreatePipeline(Shader vertexShader, Shader fragmentShader)
    {
        Check(Native.nreCreatePipeline(vertexShader, fragmentShader, out Pipeline pipeline));
        return pipeline;
    }
    public static void DestroyPipeline(Pipeline pipeline) => Check(Native.nreDestroyPipeline(pipeline));
    public static void BindPipeline(Pipeline pipeline) => Check(Native.nreBindPipeline(pipeline));

    // ---------------------------------------------------------------------------------------------
    // Models and instances
    // ---------------------------------------------------------------------------------------------

    /// <summary>Loads a model file. It isn't drawn by itself: draw it through instances (see renderer.h).</summary>
    public static Model CreateModel(string modelPath)
    {
        Check(Native.nreCreateModel(modelPath, out Model model));
        return model;
    }
    /// <summary>
    /// A model from your own triangles (counter-clockwise front faces, indices into vertices). Vertices with a zero
    /// normal get one computed from their triangles. No material: the default one. The spans are passed to the
    /// library as they are (no copy here) and it copies them during the call.
    /// </summary>
    public static Model CreateMeshModel(ReadOnlySpan<Vertex> vertices, ReadOnlySpan<uint> indices, Material? material = null)
    {
        fixed (Vertex* v = vertices)
        fixed (uint* i = indices)
        {
            Check(Native.nreCreateMeshModel(v, (uint)vertices.Length, i, (uint)indices.Length,
                                            material ?? Material.None, out Model model));
            return model;
        }
    }
    /// <summary>Instances of a destroyed model stay valid but draw nothing.</summary>
    public static void DestroyModel(Model model) => Check(Native.nreDestroyModel(model));
    public static Instance CreateInstance(Model model)
    {
        Check(Native.nreCreateInstance(model, out Instance instance));
        return instance;
    }
    /// <summary>Also removes it from every scene it's in.</summary>
    public static void DestroyInstance(Instance instance) => Check(Native.nreDestroyInstance(instance));
    public static void SetInstanceTransform(Instance instance, Transform transform) =>
        Check(Native.nreSetInstanceTransform(instance, in transform));
    public static Transform GetInstanceTransform(Instance instance)
    {
        Check(Native.nreGetInstanceTransform(instance, out Transform transform));
        return transform;
    }
    public static Mat4 GetInstanceMatrix(Instance instance)
    {
        Check(Native.nreGetInstanceMatrix(instance, out Mat4 matrix));
        return matrix;
    }
    public static void TranslateInstance(Instance instance, Vec3 offset) => Check(Native.nreTranslateInstance(instance, offset));
    /// <summary>Angle is in radians.</summary>
    public static void RotateInstance(Instance instance, float angle, Vec3 axis) => Check(Native.nreRotateInstance(instance, angle, axis));
    public static void ScaleInstance(Instance instance, Vec3 factor) => Check(Native.nreScaleInstance(instance, factor));
    /// <summary>A hidden instance stays in its scenes but isn't drawn.</summary>
    public static void SetVisibility(Instance instance, bool visible) => Check(Native.nreSetVisibility(instance, visible));
    /// <summary>Whether the instance casts sun shadows (it still receives them). Default true; e.g. off for grass.</summary>
    public static void SetCastsShadows(Instance instance, bool castsShadows) =>
        Check(Native.nreSetCastsShadows(instance, castsShadows));
    /// <summary>FunctionDrawMode only, between RenderFrame and EndFrame. No transform: the identity transform.</summary>
    public static void DrawModel(Model model, Transform? transform = null)
    {
        if (transform is Transform t) Check(Native.nreDrawModel(model, &t));
        else Check(Native.nreDrawModel(model, null));
    }
    /// <summary>FunctionDrawMode only. Uses the instance's transform; a hidden instance isn't drawn.</summary>
    public static void DrawInstance(Instance instance) => Check(Native.nreDrawInstance(instance));

    // ---------------------------------------------------------------------------------------------
    // Scenes
    // ---------------------------------------------------------------------------------------------

    public static Scene CreateScene()
    {
        Check(Native.nreCreateScene(out Scene scene));
        return scene;
    }
    public static void DestroyScene(Scene scene) => Check(Native.nreDestroyScene(scene));
    public static void BindScene(Scene scene) => Check(Native.nreBindScene(scene));
    public static Scene GetBoundScene()
    {
        Check(Native.nreGetBoundScene(out Scene scene));
        return scene;
    }
    /// <summary>Adds to the bound scene. Adding an instance that's already in it does nothing.</summary>
    public static void AddInstanceToScene(Instance instance) => Check(Native.nreAddInstanceToScene(instance));
    public static void RemoveInstanceFromScene(Instance instance) => Check(Native.nreRemoveInstanceFromScene(instance));

    // ---------------------------------------------------------------------------------------------
    // Textures and materials. A null / None texture uses the default texture.
    // ---------------------------------------------------------------------------------------------

    /// <summary>
    /// Loads an image file (PNG, JPEG, ...). srgb: true for color images (base color), false for data maps (normal
    /// and metallic-roughness maps).
    /// </summary>
    public static Texture CreateTexture(string path, bool srgb = true)
    {
        Check(Native.nreCreateTexture(path, srgb, out Texture texture));
        return texture;
    }
    /// <summary>Give the materials that use it another texture first: they keep pointing at it.</summary>
    public static void DestroyTexture(Texture texture) => Check(Native.nreDestroyTexture(texture));
    /// <summary>metallicTexture and normalTexture are stored but not used yet (see renderer.h).</summary>
    public static Material CreateMaterial(Texture? baseTexture = null, Texture? metallicTexture = null, Texture? normalTexture = null)
    {
        Check(Native.nreCreateMaterial(baseTexture ?? Texture.None, metallicTexture ?? Texture.None,
                                       normalTexture ?? Texture.None, out Material material));
        return material;
    }
    /// <summary>Models still using it fall back to the default material.</summary>
    public static void DestroyMaterial(Material material) => Check(Native.nreDestroyMaterial(material));
    /// <summary>Waits for the GPU to finish the frames in flight, so don't call it every frame.</summary>
    public static void SetMaterialBaseTexture(Material material, Texture? texture) =>
        Check(Native.nreSetMaterialBaseTexture(material, texture ?? Texture.None));
    /// <summary>The model's material slots, in the file's material order. Empty for a model without materials.</summary>
    public static Material[] GetModelMaterials(Model model)
    {
        uint count = 0;
        Check(Native.nreGetModelMaterials(model, ref count, null));
        if (count == 0) return [];
        var materials = new Material[count];
        fixed (Material* p = materials) Check(Native.nreGetModelMaterials(model, ref count, p));
        return count == materials.Length ? materials : materials[..(int)count];
    }
    /// <summary>Makes every mesh of the model use this material, replacing all of its slots.</summary>
    public static void SetModelMaterial(Model model, Material material) => Check(Native.nreSetModelMaterial(model, material));
    /// <summary>Replaces slot <paramref name="index"/> (an index into <see cref="GetModelMaterials"/>).</summary>
    public static void ReplaceModelMaterial(Model model, uint index, Material material) =>
        Check(Native.nreReplaceModelMaterial(model, index, material));

    // ---------------------------------------------------------------------------------------------
    // Lights and the sun. No color: white.
    // ---------------------------------------------------------------------------------------------

    /// <summary>direction is the way the light travels, e.g. (0, -1, 0) shines straight down.</summary>
    public static Light CreateDirectionalLight(Vec3 direction, Vec3? color = null, float intensity = 3.0f)
    {
        Check(Native.nreCreateDirectionalLight(direction, color ?? Vec3.One, intensity, out Light light));
        return light;
    }
    /// <summary>Falls off with 1/distance².</summary>
    public static Light CreatePointLight(Vec3 position, Vec3? color = null, float intensity = 10.0f)
    {
        Check(Native.nreCreatePointLight(position, color ?? Vec3.One, intensity, out Light light));
        return light;
    }
    /// <summary>Also removes it from every scene it's in.</summary>
    public static void DestroyLight(Light light) => Check(Native.nreDestroyLight(light));
    /// <summary>Adds to the bound scene. Adding a light that's already in it does nothing.</summary>
    public static void AddLightToScene(Light light) => Check(Native.nreAddLightToScene(light));
    public static void RemoveLightFromScene(Light light) => Check(Native.nreRemoveLightFromScene(light));
    /// <summary>Only used by point lights.</summary>
    public static void SetLightPosition(Light light, Vec3 position) => Check(Native.nreSetLightPosition(light, position));
    /// <summary>Only used by directional lights.</summary>
    public static void SetLightDirection(Light light, Vec3 direction) => Check(Native.nreSetLightDirection(light, direction));
    public static void SetLightColor(Light light, Vec3 color, float intensity) => Check(Native.nreSetLightColor(light, color, intensity));
    /// <summary>The bound scene's sun: one directional light kept apart from the others (see renderer.h).</summary>
    public static void SetSun(Vec3 direction, Vec3? color = null, float intensity = 3.0f) =>
        Check(Native.nreSetSun(direction, color ?? Vec3.One, intensity));
    /// <summary>Keeps the color and intensity, e.g. for a day/night cycle.</summary>
    public static void SetSunDirection(Vec3 direction) => Check(Native.nreSetSunDirection(direction));
    public static Vec3 GetSunDirection()
    {
        Check(Native.nreGetSunDirection(out Vec3 direction));
        return direction;
    }

    // ---------------------------------------------------------------------------------------------
    // Scene lighting, shadows, haze, exposure and sky color, on the bound scene. renderer.h's defaults:
    // Small: shadows 20 m, haze off. Landscape: shadows 500 m, haze 8000 m / 300 m. Exposure 1, sky (0.1, 0.3, 0.6).
    // ---------------------------------------------------------------------------------------------

    /// <summary>Also resets shadow distance, haze and the sky to the mode's defaults (see renderer.h).</summary>
    public static void SetSceneLighting(SceneLighting lighting) => Check(Native.nreSetSceneLighting((int)lighting));
    /// <summary>Throws <see cref="NreException"/> (Unknown) if the library returns a mode these bindings don't know.</summary>
    public static SceneLighting GetSceneLighting()
    {
        Check(Native.nreGetSceneLighting(out int lighting));
        if (!Enum.IsDefined((SceneLighting)lighting))
            throw new NreException(ErrorCode.Unknown,
                $"nreGetSceneLighting returned {lighting}, which these bindings don't know");
        return (SceneLighting)lighting;
    }
    /// <summary>Meters from the camera; 0 turns sun shadows off, negative throws InvalidArgument.</summary>
    public static void SetShadowDistance(float meters) => Check(Native.nreSetShadowDistance(meters));
    /// <summary>
    /// visibility: meters at which the haze hides almost everything, 0 = off. height: the haze layer's thickness
    /// above y = 0, greater than 0.
    /// </summary>
    public static void SetHaze(float visibility, float height = 300.0f) => Check(Native.nreSetHaze(visibility, height));
    /// <summary>Greater than 0; 2 is twice as bright. In Landscape it multiplies the automatic exposure.</summary>
    public static void SetExposure(float exposure) => Check(Native.nreSetExposure(exposure));
    /// <summary>The background without a skybox (Small), linear 0..1.</summary>
    public static void SetSkyColor(Vec3 color) => Check(Native.nreSetSkyColor(color));

    // ---------------------------------------------------------------------------------------------
    // Cubemaps and the skybox
    // ---------------------------------------------------------------------------------------------

    /// <summary>Six square images of the same size, in the OpenGL order: +X, -X, +Y, -Y, +Z, -Z.</summary>
    public static Cubemap CreateCubemap(string right, string left, string top, string bottom, string front, string back)
    {
        Check(Native.nreCreateCubemap(right, left, top, bottom, front, back, out Cubemap cubemap));
        return cubemap;
    }
    /// <summary>Waits for the GPU to finish the frames in flight. Scenes using it go back to having no skybox.</summary>
    public static void DestroyCubemap(Cubemap cubemap) => Check(Native.nreDestroyCubemap(cubemap));
    /// <summary>The bound scene's sky (3D only). null / None removes it.</summary>
    public static void SetSkybox(Cubemap? cubemap) => Check(Native.nreSetSkybox(cubemap ?? Cubemap.None));

    // ---------------------------------------------------------------------------------------------
    // Camera. Angles are in radians.
    // ---------------------------------------------------------------------------------------------

    public static void SetCameraPosition(Vec3 position) => Check(Native.nreSetCameraPosition(position));
    public static Vec3 GetCameraPosition()
    {
        Check(Native.nreGetCameraPosition(out Vec3 position));
        return position;
    }
    public static void MoveCamera(Vec3 offset) => Check(Native.nreMoveCamera(offset));
    public static void MoveCameraRelative(Vec3 offset) => Check(Native.nreMoveCameraRelative(offset));
    public static void SetCameraRotation(float yaw, float pitch) => Check(Native.nreSetCameraRotation(yaw, pitch));
    public static void RotateCamera(float yawDelta, float pitchDelta) => Check(Native.nreRotateCamera(yawDelta, pitchDelta));
    public static void CameraLookAt(Vec3 target) => Check(Native.nreCameraLookAt(target));
    public static Vec3 GetCameraForward()
    {
        Check(Native.nreGetCameraForward(out Vec3 forward));
        return forward;
    }
    public static Mat4 GetViewMatrix()
    {
        Check(Native.nreGetViewMatrix(out Mat4 view));
        return view;
    }
    /// <summary>
    /// Vulkan conventions: clip-space +Y down, reverse-Z with an infinite far plane (see renderer.h). Code written for
    /// OpenGL conventions has to flip Y back.
    /// </summary>
    public static Mat4 GetProjectionMatrix()
    {
        Check(Native.nreGetProjectionMatrix(out Mat4 projection));
        return projection;
    }
    /// <summary>
    /// The nearest visible instance of the bound scene under a pixel (<see cref="GetMousePosition"/> units), or null.
    /// Render3D only.
    /// </summary>
    public static RayHit? PickInstance(Vec2 windowPos)
    {
        Check(Native.nrePickInstance(windowPos, out bool hit, out RayHit rayHit));
        return hit ? rayHit : null;
    }

    // ---------------------------------------------------------------------------------------------
    // Timing
    // ---------------------------------------------------------------------------------------------

    /// <summary>Frames per second, averaged over the last half second.</summary>
    public static float GetFPS()
    {
        Check(Native.nreGetFPS(out float fps));
        return fps;
    }
    /// <summary>Seconds between the last two presented frames.</summary>
    public static float GetDeltaTime()
    {
        Check(Native.nreGetDeltaTime(out float deltaTime));
        return deltaTime;
    }

    // ---------------------------------------------------------------------------------------------
    // Input. Deltas cover the time between the last two PollEvents calls; pressed / released are true
    // for the one frame after the PollEvents call that saw it happen (see renderer.h).
    // ---------------------------------------------------------------------------------------------

    public static bool IsMouseButtonDown(MouseButton button)
    {
        Check(Native.nreIsMouseButtonDown(button, out bool down));
        return down;
    }
    public static bool IsMouseButtonPressed(MouseButton button)
    {
        Check(Native.nreIsMouseButtonPressed(button, out bool pressed));
        return pressed;
    }
    public static bool IsMouseButtonReleased(MouseButton button)
    {
        Check(Native.nreIsMouseButtonReleased(button, out bool released));
        return released;
    }
    /// <summary>How far the cursor moved, in pixels. +x is right, +y is down.</summary>
    public static Vec2 GetMouseDelta()
    {
        Check(Native.nreGetMouseDelta(out Vec2 delta));
        return delta;
    }
    /// <summary>Scroll wheel steps. Positive means scrolling up / away from you.</summary>
    public static float GetScrollDelta()
    {
        Check(Native.nreGetScrollDelta(out float delta));
        return delta;
    }
    /// <summary>Cursor position in pixels from the window's top-left corner.</summary>
    public static Vec2 GetMousePosition()
    {
        Check(Native.nreGetMousePosition(out Vec2 position));
        return position;
    }
    public static void SetCursorMode(CursorMode mode) => Check(Native.nreSetCursorMode(mode));
    public static bool IsKeyDown(Key key)
    {
        Check(Native.nreIsKeyDown(key, out bool down));
        return down;
    }
    public static bool IsKeyPressed(Key key)
    {
        Check(Native.nreIsKeyPressed(key, out bool pressed));
        return pressed;
    }
    public static bool IsKeyReleased(Key key)
    {
        Check(Native.nreIsKeyReleased(key, out bool released));
        return released;
    }
}
