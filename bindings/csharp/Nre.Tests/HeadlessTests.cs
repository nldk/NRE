using System.Numerics;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text.RegularExpressions;
using Nre;

// The renderer is one global state, so run the tests one at a time.
[assembly: CollectionBehavior(DisableTestParallelization = true)]

namespace Nre.Tests;

internal static class ChildProcess
{
    // Runs this test assembly's Main (Program.cs) in a new process.
    internal static (int ExitCode, string Stdout, string Stderr) Run(string probe, string? libDir = null)
    {
        string dll = typeof(Program).Assembly.Location;
        var psi = new System.Diagnostics.ProcessStartInfo("dotnet", ["exec", dll, probe])
        {
            RedirectStandardOutput = true,
            RedirectStandardError = true,
        };
        if (libDir != null) psi.Environment["NRE_LIB_DIR"] = libDir;
        using var p = System.Diagnostics.Process.Start(psi)!;
        var stdout = p.StandardOutput.ReadToEndAsync();
        var stderr = p.StandardError.ReadToEndAsync();
        if (!p.WaitForExit(30_000)) { p.Kill(); throw new TimeoutException(probe); }
        return (p.ExitCode, stdout.Result, stderr.Result);
    }
}

internal static class Setup
{
    internal static string RepoDir([CallerFilePath] string thisFile = "") =>
        Path.GetFullPath(Path.Combine(Path.GetDirectoryName(thisFile)!, "..", "..", ".."));

    // Without NRE_LIB_DIR, use the repo's Debug build (the one that checks misuse like calls before init).
    [ModuleInitializer]
    internal static void DefaultLibDir()
    {
        if (string.IsNullOrEmpty(Environment.GetEnvironmentVariable("NRE_LIB_DIR")))
            Environment.SetEnvironmentVariable("NRE_LIB_DIR", Path.Combine(RepoDir(), "cmake-build-debug", "bin"));
    }
}

// Everything here works without a window or GPU: no test calls Init.
public class HeadlessTests
{
    [Fact]
    public void VersionMatchesTheTargetedAbi()
    {
        Assert.Equal((0u, 3u), (Native.AbiMajor, Native.AbiMinor));
        var (major, minor, _) = NRE.GetVersion();
        Assert.Equal((0u, 3u), (major, minor));
        if (OperatingSystem.IsLinux()) Assert.Equal("libnre_c.so.0.3", Native.LibraryFileName);
    }

    // A library of another version under the expected file name (here: the 0.2 build, if it's still lying around
    // in the build directory) is rejected on the first call.
    [Fact]
    public void OtherVersionsAreRejected()
    {
        string old = Path.Combine(Environment.GetEnvironmentVariable("NRE_LIB_DIR")!, "libnre_c.so.0.2");
        if (!OperatingSystem.IsLinux() || !File.Exists(old)) return;   // nothing to test against

        string dir = Directory.CreateTempSubdirectory("nre-version-test").FullName;
        try
        {
            File.CreateSymbolicLink(Path.Combine(dir, Native.LibraryFileName), Path.GetFullPath(old));
            var (exitCode, stdout, stderr) = ChildProcess.Run("get-version", dir);
            Assert.True(exitCode == 0, stderr);
            Assert.Matches(@"^CAUGHT LibraryLoadFailed .*version 0\.2\.\d+, but these bindings target 0\.3\.x$", stdout.Trim());
        }
        finally
        {
            Directory.Delete(dir, recursive: true);
        }
    }

    [Theory]
    [InlineData(ErrorCode.Ok, "Ok")]
    [InlineData(ErrorCode.FileNotFound, "FileNotFound")]
    [InlineData(ErrorCode.NotInitialized, "NotInitialized")]
    [InlineData(ErrorCode.LibraryLoadFailed, "LibraryLoadFailed")]
    [InlineData(ErrorCode.InvalidArgument, "InvalidArgument")]
    [InlineData(ErrorCode.Unknown, "UnknownError")]
    public void ErrorCodeToStringUsesTheLibrarysNames(ErrorCode code, string expected) =>
        Assert.Equal(expected, NRE.ErrorCodeToString(code));

    [Fact]
    public void ErrorCodeToStringIsSafeForUndefinedValues() =>
        Assert.Equal("UnknownError", NRE.ErrorCodeToString((ErrorCode)int.MinValue));

    [Theory]
    [InlineData(9, ErrorCode.NotInitialized)]
    [InlineData(29, ErrorCode.LibraryLoadFailed)]
    [InlineData(30, ErrorCode.InvalidArgument)]
    [InlineData(31, ErrorCode.Unknown)]
    [InlineData(999, ErrorCode.Unknown)]
    [InlineData(-1, ErrorCode.Unknown)]
    [InlineData(0x7FFFFFFF, ErrorCode.Unknown)]
    public void UnknownCodesMapToUnknown(int raw, ErrorCode expected) =>
        Assert.Equal(expected, NRE.ToErrorCode(raw));

    // Before setRenderConfig() there's no renderer context, and the renderer's error policy is then Panic: it
    // prints the error and aborts, so the C call never returns NRE_ERROR_NOT_INITIALIZED. Run it in a child
    // process and accept either outcome: the exception (if the library ever returns the code) or the abort
    // with the NotInitialized message on stderr (what the 0.2 and 0.3 Debug builds do).
    [Fact]
    public void CallsBeforeSetRenderConfigReportNotInitialized()
    {
        var (exitCode, stdout, stderr) = ChildProcess.Run("create-model-before-config");
        if (exitCode == 0)
        {
            Assert.Matches(@"^CAUGHT NotInitialized \S", stdout.Trim());
        }
        else
        {
            Assert.Contains("[NRE] NotInitialized: createModel() was called before setRenderConfig()", stderr);
            Assert.DoesNotContain("RETURNED", stdout);
        }
    }

    // After setRenderConfig() (no init, so no window) errors follow the chosen policy; Return gives the exception.
    // This leaves the renderer configured for the rest of the test process (never call Shutdown without Init:
    // it uses the Vulkan device). No other in-process test needs it unconfigured.
    private static void EnsureConfigured()
    {
        try { NRE.SetRenderConfig(320, 240, "unused", errorPolicy: RenderErrorPolicy.Return); }
        catch (NreException e) when (e.Code == ErrorCode.AlreadyInitialized) { }
    }

    private static void AssertNreError(ErrorCode expected, Action call)
    {
        var e = Assert.Throws<NreException>(call);
        Assert.Equal(expected, e.Code);
        Assert.NotEmpty(e.NativeMessage);
        Assert.Equal(e.NativeMessage, NRE.GetLastErrorMessage());
        Assert.Equal($"{expected}: {e.NativeMessage}", e.Message);
    }

    [Fact]
    public void SecondSetRenderConfigThrowsAlreadyInitialized()
    {
        EnsureConfigured();
        AssertNreError(ErrorCode.AlreadyInitialized, () => NRE.SetRenderConfig(320, 240, "unused"));
        Assert.Contains("setRenderConfig() was called twice", NRE.GetLastErrorMessage());
    }

    [Fact]
    public void EmptyHandlesThrowInvalidHandle()
    {
        EnsureConfigured();
        AssertNreError(ErrorCode.InvalidHandle, () => NRE.GetInstanceTransform(Instance.None));
        Assert.Contains("empty", NRE.GetLastErrorMessage());
        AssertNreError(ErrorCode.InvalidHandle, () => NRE.TranslateInstance(Instance.None, (1f, 0f, 0f)));
        AssertNreError(ErrorCode.InvalidHandle, () => NRE.GetModelMaterials(Model.None));
        AssertNreError(ErrorCode.InvalidHandle, () => NRE.SetCastsShadows(Instance.None, false));
    }

    [Fact]
    public void StructSizesMatchNreH()
    {
        Assert.Equal(8, Unsafe.SizeOf<Vec2>());
        Assert.Equal(12, Unsafe.SizeOf<Vec3>());
        Assert.Equal(16, Unsafe.SizeOf<Quat>());
        Assert.Equal(64, Unsafe.SizeOf<Mat4>());
        Assert.Equal(40, Unsafe.SizeOf<Transform>());
        Assert.Equal(8, Unsafe.SizeOf<Shader>());
        Assert.Equal(8, Unsafe.SizeOf<Pipeline>());
        Assert.Equal(8, Unsafe.SizeOf<Model>());
        Assert.Equal(8, Unsafe.SizeOf<Instance>());
        Assert.Equal(8, Unsafe.SizeOf<Light>());
        Assert.Equal(8, Unsafe.SizeOf<Scene>());
        Assert.Equal(8, Unsafe.SizeOf<Texture>());
        Assert.Equal(8, Unsafe.SizeOf<Material>());
        Assert.Equal(8, Unsafe.SizeOf<Cubemap>());
        Assert.Equal(32, Unsafe.SizeOf<Vertex>());
        Assert.Equal(24, Unsafe.SizeOf<RayHit>());
    }

    [Fact]
    public void FieldOrderMatchesNreH()
    {
        var t = new Transform(new Vec3(1, 2, 3), new Quat(4, 5, 6, 7), new Vec3(8, 9, 10));
        float[] floats = MemoryMarshal.Cast<Transform, float>(MemoryMarshal.CreateSpan(ref t, 1)).ToArray();
        Assert.Equal([1, 2, 3, 4, 5, 6, 7, 8, 9, 10], floats);

        // NreVertex: position at 0, normal at 12, uv at 24.
        var v = new Vertex((1f, 2f, 3f), (4f, 5f, 6f), (7f, 8f));
        float[] vertexFloats = MemoryMarshal.Cast<Vertex, float>(MemoryMarshal.CreateSpan(ref v, 1)).ToArray();
        Assert.Equal([1, 2, 3, 4, 5, 6, 7, 8], vertexFloats);
        Assert.Equal(new Vertex((1f, 2f, 3f), Vec3.Zero, Vec2.Zero), new Vertex((1f, 2f, 3f)));

        // NreRayHit: instance at 0 (index, generation), distance at 8, point at 12.
        var hit = new RayHit(new Instance(11, 22), 3.5f, (4f, 5f, 6f));
        ReadOnlySpan<byte> bytes = MemoryMarshal.AsBytes(MemoryMarshal.CreateSpan(ref hit, 1));
        Assert.Equal(11u, BitConverter.ToUInt32(bytes[0..]));
        Assert.Equal(22u, BitConverter.ToUInt32(bytes[4..]));
        Assert.Equal(3.5f, BitConverter.ToSingle(bytes[8..]));
        Assert.Equal([4f, 5f, 6f], MemoryMarshal.Cast<byte, float>(bytes[12..]).ToArray());

        var m = new Model(11, 22);
        uint[] uints = MemoryMarshal.Cast<Model, uint>(MemoryMarshal.CreateSpan(ref m, 1)).ToArray();
        Assert.Equal([11u, 22u], uints);
    }

    [Fact]
    public void EmptyHandlesAreAllZeros()
    {
        Assert.True(Texture.None.IsNone);
        Assert.Equal(default, Cubemap.None);
        Assert.False(new Model(1, 0).IsNone);
        Assert.Equal("Model(1, 0)", new Model(1, 0).ToString());
    }

    // Every enum has exactly the values nre.h declares (parsed from the header, minus the *_MAX_ENUM sentinels).
    [Theory]
    [InlineData("NreErrorCode", typeof(ErrorCode))]
    [InlineData("NreShaderType", typeof(ShaderType))]
    [InlineData("NreRenderMode", typeof(RenderMode))]
    [InlineData("NreApiMode", typeof(ApiMode))]
    [InlineData("NreSceneLighting", typeof(SceneLighting))]
    [InlineData("NreRenderErrorPolicy", typeof(RenderErrorPolicy))]
    [InlineData("NreMouseButton", typeof(MouseButton))]
    [InlineData("NreCursorMode", typeof(CursorMode))]
    [InlineData("NreKey", typeof(Key))]
    public void EnumValuesMatchNreH(string cName, Type csType)
    {
        string header = File.ReadAllText(Path.Combine(Setup.RepoDir(), "include", "nre.h"));
        var body = Regex.Match(header, @"typedef enum " + cName + @" \{(.*?)\} " + cName + ";", RegexOptions.Singleline);
        Assert.True(body.Success, $"{cName} not found in nre.h");
        var cValues = Regex.Matches(Regex.Replace(body.Groups[1].Value, "//.*", ""), @"(NRE_\w+)\s*=\s*(0x[0-9A-Fa-f]+|\d+)")
            .Where(m => !m.Groups[1].Value.EndsWith("_MAX_ENUM"))
            .Select(m => Convert.ToInt32(m.Groups[2].Value, m.Groups[2].Value.StartsWith("0x") ? 16 : 10))
            .Order().ToArray();
        var csValues = Enum.GetValues(csType).Cast<object>().Select(Convert.ToInt32).Order().ToArray();
        Assert.Equal(cValues, csValues);
    }

    // Native declares exactly the functions nre.h exports.
    [Fact]
    public void NativeDeclaresEveryFunctionOfNreH()
    {
        string header = File.ReadAllText(Path.Combine(Setup.RepoDir(), "include", "nre.h"));
        var cFunctions = Regex.Matches(header, @"NRE_C_EXPORT[^;(]*?\b(nre\w+)\s*\(")
            .Select(m => m.Groups[1].Value).Order().ToArray();
        var csFunctions = typeof(Native).GetMethods(BindingFlags.Static | BindingFlags.NonPublic)
            .Select(m => m.Name).Where(n => n.StartsWith("nre")).Distinct().Order().ToArray();
        Assert.Equal(89, cFunctions.Length);
        Assert.Equal(cFunctions, csFunctions);
    }

    [Fact]
    public void MathHelpers()
    {
        Vec3 a = (1f, 2f, 3f);
        Assert.Equal(new Vec3(2, 4, 6), a + a);
        Assert.Equal(new Vec3(0, 0, 0), a - a);
        Assert.Equal(new Vec3(3, 6, 9), a * 3);
        Assert.Equal(new Vec3(3, 6, 9), 3 * a);
        Vector3 n = a;
        Assert.Equal(new Vector3(1, 2, 3), n);
        Assert.Equal(a, (Vec3)new Vector3(1, 2, 3));

        Assert.Equal(new Quat(0, 0, 0, 1), Quat.Identity);
        Assert.Equal(Transform.Identity, new Transform(Vec3.Zero));

        var translate = new Mat4([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 5, 6, 7, 1]);
        Assert.Equal(5, translate[3, 0]);
        Assert.Equal(Matrix4x4.CreateTranslation(5, 6, 7), translate.ToMatrix4x4());
        Assert.Equal(Matrix4x4.Identity, Mat4.Identity.ToMatrix4x4());
    }
}
