using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

// Structs cross the boundary as they are (they're unmanaged and have nre.h's layout); only bool and strings
// are converted, by the LibraryImport source generator.
[assembly: DisableRuntimeMarshalling]

namespace Nre;

/// <summary>
/// The raw C API (nre.h), one-to-one. Functions return the NreErrorCode as an int so an unknown value can't
/// become an unnamed enum value by accident; <see cref="NRE"/> turns it into an exception.
/// C bool is 1 byte, hence MarshalAs(U1). Returned strings are static / library-owned, so they come back as
/// byte* (a string return would make the marshaller free them).
/// </summary>
internal static unsafe partial class Native
{
    internal const string Lib = "nre_c";

    /// <summary>The ABI these bindings target: major.minor of the library must match.</summary>
    internal const uint AbiMajor = 0, AbiMinor = 3;

    static Native() => NativeLibrary.SetDllImportResolver(typeof(Native).Assembly, Resolve);

    internal static string LibraryFileName =>
        OperatingSystem.IsWindows() ? "nre_c.dll" :
        OperatingSystem.IsMacOS() ? "libnre_c.0.3.dylib" :
        "libnre_c.so.0.3";

    // Loads libnre_c from NRE_LIB_DIR if set, else by its versioned name through the normal search,
    // then checks its version before any binding uses it.
    private static IntPtr Resolve(string libraryName, Assembly assembly, DllImportSearchPath? searchPath)
    {
        if (libraryName != Lib) return IntPtr.Zero;

        string? dir = Environment.GetEnvironmentVariable("NRE_LIB_DIR");
        IntPtr handle;
        try
        {
            handle = string.IsNullOrEmpty(dir)
                ? NativeLibrary.Load(LibraryFileName, assembly, searchPath)
                : NativeLibrary.Load(Path.Combine(dir, LibraryFileName));
        }
        catch (DllNotFoundException e)
        {
            string where = string.IsNullOrEmpty(dir) ? "the library search path" : $"NRE_LIB_DIR ({dir})";
            throw new DllNotFoundException(
                $"Could not load {LibraryFileName} from {where}. Set NRE_LIB_DIR to the directory holding " +
                $"libnre_c (e.g. <repo>/cmake-build-debug/bin). {e.Message}", e);
        }

        var getVersion = (delegate* unmanaged<uint*, uint*, uint*, void>)NativeLibrary.GetExport(handle, "nreGetVersion");
        uint major, minor, patch;
        getVersion(&major, &minor, &patch);
        if (major != AbiMajor || minor != AbiMinor)
        {
            NativeLibrary.Free(handle);
            throw new NreException(ErrorCode.LibraryLoadFailed,
                $"{LibraryFileName} is version {major}.{minor}.{patch}, but these bindings target {AbiMajor}.{AbiMinor}.x");
        }
        return handle;
    }

    // Errors and version
    [LibraryImport(Lib)] internal static partial byte* nreGetLastErrorMessage();
    [LibraryImport(Lib)] internal static partial byte* nreErrorCodeToString(int code);
    [LibraryImport(Lib)] internal static partial void nreGetVersion(out uint major, out uint minor, out uint patch);

    // Setup and frames
    [LibraryImport(Lib, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int nreSetRenderConfig(RenderMode mode, ApiMode apiMode, RenderErrorPolicy errorPolicy,
        [MarshalAs(UnmanagedType.U1)] bool manualFramePresentationAndSubmission,
        int windowWidth, int windowHeight, string windowTitle,
        [MarshalAs(UnmanagedType.U1)] bool resizable, [MarshalAs(UnmanagedType.U1)] bool showFpsInTitle);
    [LibraryImport(Lib)] internal static partial int nreInit();
    [LibraryImport(Lib)] internal static partial int nreShutdown();
    [LibraryImport(Lib)] internal static partial int nreRenderFrame();
    [LibraryImport(Lib)] internal static partial int nreEndFrame();
    [LibraryImport(Lib)] internal static partial int nrePresentFrame();
    [LibraryImport(Lib)] internal static partial int nreSubmitFrame();
    [LibraryImport(Lib)] internal static partial int nreWindowShouldClose([MarshalAs(UnmanagedType.U1)] out bool shouldClose);
    [LibraryImport(Lib)] internal static partial int nrePollEvents();

    // Shaders and pipelines
    [LibraryImport(Lib, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int nreCreateShader(string shaderPath, ShaderType type, out Shader shader);
    [LibraryImport(Lib)] internal static partial int nreDestroyShader(Shader shader);
    [LibraryImport(Lib)] internal static partial int nreCreatePipeline(Shader vertexShader, Shader fragmentShader, out Pipeline pipeline);
    [LibraryImport(Lib)] internal static partial int nreDestroyPipeline(Pipeline pipeline);
    [LibraryImport(Lib)] internal static partial int nreBindPipeline(Pipeline pipeline);

    // Models and instances
    [LibraryImport(Lib, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int nreCreateModel(string modelPath, out Model model);
    // The arrays are copied during the call; material None = the default one.
    [LibraryImport(Lib)] internal static partial int nreCreateMeshModel(Vertex* vertices, uint vertexCount, uint* indices,
        uint indexCount, Material material, out Model model);
    [LibraryImport(Lib)] internal static partial int nreDestroyModel(Model model);
    [LibraryImport(Lib)] internal static partial int nreCreateInstance(Model model, out Instance instance);
    [LibraryImport(Lib)] internal static partial int nreDestroyInstance(Instance instance);
    [LibraryImport(Lib)] internal static partial int nreSetInstanceTransform(Instance instance, in Transform transform);
    [LibraryImport(Lib)] internal static partial int nreGetInstanceTransform(Instance instance, out Transform transform);
    [LibraryImport(Lib)] internal static partial int nreGetInstanceMatrix(Instance instance, out Mat4 matrix);
    [LibraryImport(Lib)] internal static partial int nreTranslateInstance(Instance instance, Vec3 offset);
    [LibraryImport(Lib)] internal static partial int nreRotateInstance(Instance instance, float angle, Vec3 axis);
    [LibraryImport(Lib)] internal static partial int nreScaleInstance(Instance instance, Vec3 factor);
    [LibraryImport(Lib)] internal static partial int nreSetVisibility(Instance instance, [MarshalAs(UnmanagedType.U1)] bool visible);
    [LibraryImport(Lib)] internal static partial int nreSetCastsShadows(Instance instance, [MarshalAs(UnmanagedType.U1)] bool castsShadows);
    // transform may be null: the identity transform.
    [LibraryImport(Lib)] internal static partial int nreDrawModel(Model model, Transform* transform);
    [LibraryImport(Lib)] internal static partial int nreDrawInstance(Instance instance);

    // Scenes
    [LibraryImport(Lib)] internal static partial int nreCreateScene(out Scene scene);
    [LibraryImport(Lib)] internal static partial int nreDestroyScene(Scene scene);
    [LibraryImport(Lib)] internal static partial int nreBindScene(Scene scene);
    [LibraryImport(Lib)] internal static partial int nreGetBoundScene(out Scene scene);
    [LibraryImport(Lib)] internal static partial int nreAddInstanceToScene(Instance instance);
    [LibraryImport(Lib)] internal static partial int nreRemoveInstanceFromScene(Instance instance);

    // Textures and materials
    [LibraryImport(Lib, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int nreCreateTexture(string path, [MarshalAs(UnmanagedType.U1)] bool srgb, out Texture texture);
    [LibraryImport(Lib)] internal static partial int nreDestroyTexture(Texture texture);
    [LibraryImport(Lib)] internal static partial int nreCreateMaterial(Texture baseTexture, Texture metallicTexture, Texture normalTexture, out Material material);
    [LibraryImport(Lib)] internal static partial int nreDestroyMaterial(Material material);
    [LibraryImport(Lib)] internal static partial int nreSetMaterialBaseTexture(Material material, Texture texture);
    // materials null: sets *count to the number of slots; else writes up to *count and sets it to how many it wrote.
    [LibraryImport(Lib)] internal static partial int nreGetModelMaterials(Model model, ref uint count, Material* materials);
    [LibraryImport(Lib)] internal static partial int nreSetModelMaterial(Model model, Material material);
    [LibraryImport(Lib)] internal static partial int nreReplaceModelMaterial(Model model, uint index, Material material);

    // Lights and the sun
    [LibraryImport(Lib)] internal static partial int nreCreateDirectionalLight(Vec3 direction, Vec3 color, float intensity, out Light light);
    [LibraryImport(Lib)] internal static partial int nreCreatePointLight(Vec3 position, Vec3 color, float intensity, out Light light);
    [LibraryImport(Lib)] internal static partial int nreDestroyLight(Light light);
    [LibraryImport(Lib)] internal static partial int nreAddLightToScene(Light light);
    [LibraryImport(Lib)] internal static partial int nreRemoveLightFromScene(Light light);
    [LibraryImport(Lib)] internal static partial int nreSetLightPosition(Light light, Vec3 position);
    [LibraryImport(Lib)] internal static partial int nreSetLightDirection(Light light, Vec3 direction);
    [LibraryImport(Lib)] internal static partial int nreSetLightColor(Light light, Vec3 color, float intensity);
    [LibraryImport(Lib)] internal static partial int nreSetSun(Vec3 direction, Vec3 color, float intensity);
    [LibraryImport(Lib)] internal static partial int nreSetSunDirection(Vec3 direction);
    [LibraryImport(Lib)] internal static partial int nreGetSunDirection(out Vec3 direction);

    // Scene lighting, shadows, haze, exposure and sky colour. NreSceneLighting crosses as an int, like the error
    // codes, so a value these bindings don't know can't become an unnamed enum value by accident.
    [LibraryImport(Lib)] internal static partial int nreSetSceneLighting(int lighting);
    [LibraryImport(Lib)] internal static partial int nreGetSceneLighting(out int lighting);
    [LibraryImport(Lib)] internal static partial int nreSetShadowDistance(float meters);
    [LibraryImport(Lib)] internal static partial int nreSetHaze(float visibility, float height);
    [LibraryImport(Lib)] internal static partial int nreSetExposure(float exposure);
    [LibraryImport(Lib)] internal static partial int nreSetSkyColor(Vec3 color);

    // Cubemaps and the skybox
    [LibraryImport(Lib, StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int nreCreateCubemap(string right, string left, string top, string bottom, string front, string back, out Cubemap cubemap);
    [LibraryImport(Lib)] internal static partial int nreDestroyCubemap(Cubemap cubemap);
    [LibraryImport(Lib)] internal static partial int nreSetSkybox(Cubemap cubemap);

    // Camera
    [LibraryImport(Lib)] internal static partial int nreSetCameraPosition(Vec3 position);
    [LibraryImport(Lib)] internal static partial int nreGetCameraPosition(out Vec3 position);
    [LibraryImport(Lib)] internal static partial int nreMoveCamera(Vec3 offset);
    [LibraryImport(Lib)] internal static partial int nreMoveCameraRelative(Vec3 offset);
    [LibraryImport(Lib)] internal static partial int nreSetCameraRotation(float yaw, float pitch);
    [LibraryImport(Lib)] internal static partial int nreRotateCamera(float yawDelta, float pitchDelta);
    [LibraryImport(Lib)] internal static partial int nreCameraLookAt(Vec3 target);
    [LibraryImport(Lib)] internal static partial int nreGetCameraForward(out Vec3 forward);
    [LibraryImport(Lib)] internal static partial int nreGetViewMatrix(out Mat4 view);
    [LibraryImport(Lib)] internal static partial int nreGetProjectionMatrix(out Mat4 projection);
    // Sets hit; when it's true rayHit holds the hit, otherwise rayHit is zeroed.
    [LibraryImport(Lib)] internal static partial int nrePickInstance(Vec2 windowPos, [MarshalAs(UnmanagedType.U1)] out bool hit, out RayHit rayHit);

    // Timing
    [LibraryImport(Lib)] internal static partial int nreGetFPS(out float fps);
    [LibraryImport(Lib)] internal static partial int nreGetDeltaTime(out float deltaTime);

    // Input
    [LibraryImport(Lib)] internal static partial int nreIsMouseButtonDown(MouseButton button, [MarshalAs(UnmanagedType.U1)] out bool down);
    [LibraryImport(Lib)] internal static partial int nreIsMouseButtonPressed(MouseButton button, [MarshalAs(UnmanagedType.U1)] out bool pressed);
    [LibraryImport(Lib)] internal static partial int nreIsMouseButtonReleased(MouseButton button, [MarshalAs(UnmanagedType.U1)] out bool released);
    [LibraryImport(Lib)] internal static partial int nreGetMouseDelta(out Vec2 delta);
    [LibraryImport(Lib)] internal static partial int nreGetScrollDelta(out float delta);
    [LibraryImport(Lib)] internal static partial int nreGetMousePosition(out Vec2 position);
    [LibraryImport(Lib)] internal static partial int nreSetCursorMode(CursorMode mode);
    [LibraryImport(Lib)] internal static partial int nreIsKeyDown(Key key, [MarshalAs(UnmanagedType.U1)] out bool down);
    [LibraryImport(Lib)] internal static partial int nreIsKeyPressed(Key key, [MarshalAs(UnmanagedType.U1)] out bool pressed);
    [LibraryImport(Lib)] internal static partial int nreIsKeyReleased(Key key, [MarshalAs(UnmanagedType.U1)] out bool released);
}
