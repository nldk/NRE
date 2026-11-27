// Port of example/main.cpp: the police car on the plains skybox, slowly rotating, with an orbit camera
// (drag with the left mouse button, scroll to zoom). Switch apiMode to try SceneMode or FunctionDrawMode.
//
//   NRE_LIB_DIR=<repo>/cmake-build-debug/bin dotnet run --project Nre.Examples.Orbit
//
// Assets come from <repo>/example/assets, shaders from NRE_SHADER_DIR (default <repo>/cmake-build-debug/example/shaders).

using System.Runtime.CompilerServices;
using Nre;

ApiMode apiMode = ApiMode.FunctionDrawMode; // or ApiMode.SceneMode

string repo = Paths.Repo();
string assets = Path.Combine(repo, "example", "assets");
string shaders = Environment.GetEnvironmentVariable("NRE_SHADER_DIR") is { Length: > 0 } dir
    ? dir
    : Path.Combine(repo, "cmake-build-debug", "example", "shaders");
// Convenience for running from the repo: the Debug build's libnre_c when NRE_LIB_DIR isn't set.
if (string.IsNullOrEmpty(Environment.GetEnvironmentVariable("NRE_LIB_DIR")))
    Environment.SetEnvironmentVariable("NRE_LIB_DIR", Path.Combine(repo, "cmake-build-debug", "bin"));

// main.cpp uses RenderErrorPolicy.Ignore; here the default (Return) is used, so a failed call throws
// NreException instead of carrying on (with Ignore, a missing shader file crashes later on).
NRE.SetRenderConfig(500, 500, "NRE C# orbit", apiMode: apiMode);
NRE.Init();
var model = NRE.CreateModel(Path.Combine(assets, "police.glb"));
var car = NRE.CreateInstance(model);
var vertex = NRE.CreateShader(Path.Combine(shaders, "vertex.vert.spv"), ShaderType.Vertex);
var frag = NRE.CreateShader(Path.Combine(shaders, "fragment.frag.spv"), ShaderType.Fragment);
var pipeline = NRE.CreatePipeline(vertex, frag);
NRE.BindPipeline(pipeline);
var scene = NRE.CreateScene();
NRE.AddInstanceToScene(car);
NRE.TranslateInstance(car, new Vec3(0, 0, -1));
string sky = Path.Combine(assets, "skyboxes", "plains");
var skybox = NRE.CreateCubemap(Path.Combine(sky, "right.png"), Path.Combine(sky, "left.png"), Path.Combine(sky, "top.png"),
                               Path.Combine(sky, "bottom.png"), Path.Combine(sky, "front.png"), Path.Combine(sky, "back.png"));
NRE.SetSkybox(skybox);

var camera = new OrbitCamera { Target = NRE.GetInstanceTransform(car).Position };
while (!NRE.WindowShouldClose())
{
    NRE.PollEvents();
    camera.Update();
    if (apiMode == ApiMode.SceneMode)
    {
        NRE.RenderFrame();
    }
    else
    {
        NRE.RenderFrame();
        NRE.DrawInstance(car);
        NRE.EndFrame();
    }

    NRE.RotateInstance(car, 0.0001f, new Vec3(0, 1, 0));
}
NRE.Shutdown();

// The orbit camera lives in the app, not the bindings: it only uses the input and camera functions.
sealed class OrbitCamera
{
    public Vec3 Target;
    public float Distance = 3.0f;
    public float Yaw;
    public float Pitch;

    const float RotateSpeed = 0.01f;
    const float ZoomStep = 0.9f;
    const float MaxPitch = 1.55f;

    public void Update()
    {
        if (NRE.IsMouseButtonDown(MouseButton.Left))
        {
            Vec2 d = NRE.GetMouseDelta();
            Yaw -= d.X * RotateSpeed;
            Pitch = Math.Clamp(Pitch + d.Y * RotateSpeed, -MaxPitch, MaxPitch);
        }

        Distance = MathF.Max(Distance * MathF.Pow(ZoomStep, NRE.GetScrollDelta()), 0.1f);

        var offset = new Vec3(MathF.Cos(Pitch) * MathF.Sin(Yaw), MathF.Sin(Pitch), MathF.Cos(Pitch) * MathF.Cos(Yaw));
        NRE.SetCameraPosition(Target + offset * Distance);
        NRE.CameraLookAt(Target);
    }
}

static class Paths
{
    // <repo>, found from this source file's location (bindings/csharp/Nre.Examples.Orbit/Program.cs), not the CWD.
    public static string Repo([CallerFilePath] string thisFile = "") =>
        Path.GetFullPath(Path.Combine(Path.GetDirectoryName(thisFile)!, "..", "..", ".."));
}
