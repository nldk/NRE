// The Go port of example/main.cpp: the police car on the plains skybox, slowly rotating, with an orbit
// camera (drag with the left mouse button to orbit, scroll to zoom).
//
// Run from bindings/go:  make run   (or see the README for the plain go run command)
package main

import (
	"log"
	"math"
	"os"
	"path/filepath"
	"runtime"

	"github.com/nldk/NRE/bindings/go/nre"
)

// GLFW (inside the renderer) must run on the main OS thread: lock main's goroutine to it before main runs.
func init() { runtime.LockOSThread() }

// The orbit camera lives in the app, not in the bindings, like in the C++ example.
type orbitCamera struct {
	target     nre.Vec3
	distance   float32
	yaw, pitch float32
}

const (
	rotateSpeed = 0.01
	zoomStep    = 0.9
	maxPitch    = 1.55
)

func (c *orbitCamera) update() {
	if must(nre.IsMouseButtonDown(nre.MouseButtonLeft)) {
		d := must(nre.GetMouseDelta())
		c.yaw -= d.X * rotateSpeed
		c.pitch = clamp(c.pitch+d.Y*rotateSpeed, -maxPitch, maxPitch)
	}

	scroll := must(nre.GetScrollDelta())
	c.distance = max(c.distance*float32(math.Pow(zoomStep, float64(scroll))), 0.1)

	sy, cy := sincos(c.yaw)
	sp, cp := sincos(c.pitch)
	offset := nre.Vec3{X: cp * sy, Y: sp, Z: cp * cy}
	check(nre.SetCameraPosition(c.target.Add(offset.Scale(c.distance))))
	check(nre.CameraLookAt(c.target))
}

const apiMode = nre.FunctionDrawMode // or nre.SceneMode

func main() {
	assetDir, shaderDir := paths()

	check(nre.SetRenderConfigFull(500, 500, "test",
		nre.Render3D, apiMode, nre.ErrorPolicyIgnore, false, true, true))
	check(nre.Init())

	model := must(nre.CreateModel(filepath.Join(assetDir, "police.glb")))
	car := must(nre.CreateInstance(model))
	vertex := must(nre.CreateShader(filepath.Join(shaderDir, "vertex.vert.spv"), nre.ShaderVertex))
	frag := must(nre.CreateShader(filepath.Join(shaderDir, "fragment.frag.spv"), nre.ShaderFragment))
	pipeline := must(nre.CreatePipeline(vertex, frag))
	check(nre.BindPipeline(pipeline))
	must(nre.CreateScene()) // the first scene is bound automatically
	check(nre.AddInstanceToScene(car))
	check(nre.TranslateInstance(car, nre.Vec3{X: 0, Y: 0, Z: -1}))

	dir := filepath.Join(assetDir, "skyboxes", "plains")
	face := func(name string) string { return filepath.Join(dir, name+".png") }
	sky := must(nre.CreateCubemap(face("right"), face("left"), face("top"),
		face("bottom"), face("front"), face("back")))
	check(nre.SetSkybox(sky))

	camera := orbitCamera{target: must(nre.GetInstanceTransform(car)).Position, distance: 3}
	for !must(nre.WindowShouldClose()) {
		check(nre.PollEvents())
		camera.update()
		switch apiMode {
		case nre.SceneMode:
			check(nre.RenderFrame())
		case nre.FunctionDrawMode:
			check(nre.RenderFrame())
			check(nre.DrawInstance(car))
			check(nre.EndFrame())
		}

		check(nre.RotateInstance(car, 0.0001, nre.Vec3{X: 0, Y: 1, Z: 0}))
	}
	check(nre.Shutdown())
}

// paths returns the asset directory (<repo>/example/assets) and the shader directory ($NRE_SHADER_DIR,
// or <repo>/cmake-build-debug/example/shaders). <repo> is found from this source file, not the working directory.
func paths() (assetDir, shaderDir string) {
	_, file, _, ok := runtime.Caller(0)
	if !ok {
		log.Fatal("can't find the example's source file to locate the repository")
	}
	repo := filepath.Join(filepath.Dir(file), "..", "..", "..", "..")
	shaderDir = os.Getenv("NRE_SHADER_DIR")
	if shaderDir == "" {
		shaderDir = filepath.Join(repo, "cmake-build-debug", "example", "shaders")
	}
	return filepath.Join(repo, "example", "assets"), shaderDir
}

// must and check stop the example on an error, like .value() in the C++ version.
func must[T any](v T, err error) T {
	check(err)
	return v
}

func check(err error) {
	if err != nil {
		log.Fatal(err)
	}
}

func clamp(v, lo, hi float32) float32 { return min(max(v, lo), hi) }

func sincos(a float32) (float32, float32) {
	s, c := math.Sincos(float64(a))
	return float32(s), float32(c)
}
