package nre

// Headless checks: none of these open a window or need a GPU.

import (
	"errors"
	"fmt"
	"os"
	"os/exec"
	"runtime"
	"strings"
	"testing"
	"unsafe"
)

func TestVersion(t *testing.T) {
	major, minor, patch := GetVersion()
	t.Logf("libnre_c %d.%d.%d", major, minor, patch)
	if err := checkVersion(); err != nil {
		t.Fatalf("version check failed: %v", err)
	}
	// These bindings target 0.3: a stale 0.2 library must not pass.
	if TargetVersionMajor != 0 || TargetVersionMinor != 3 || major != 0 || minor != 3 {
		t.Fatalf("loaded %d.%d, bindings target %d.%d; want 0.3", major, minor, TargetVersionMajor, TargetVersionMinor)
	}
	for _, v := range [][3]uint32{{0, 3, 0}, {0, 3, 7}} {
		if err := versionError(v[0], v[1], v[2]); err != nil {
			t.Errorf("version %v rejected: %v", v, err)
		}
	}
	for _, v := range [][3]uint32{{0, 2, 0}, {0, 2, 9}, {0, 4, 0}, {1, 3, 0}} {
		if err := versionError(v[0], v[1], v[2]); !errors.Is(err, ErrLibraryLoadFailed) {
			t.Errorf("version %v: got %v, want ErrLibraryLoadFailed", v, err)
		}
	}
}

func TestErrorCodeToString(t *testing.T) {
	cases := map[ErrorCode]string{
		OK:                   "Ok",
		ErrFileNotFound:      "FileNotFound",
		ErrNotInitialized:    "NotInitialized",
		ErrLibraryLoadFailed: "LibraryLoadFailed",
		ErrInvalidArgument:   "InvalidArgument",
	}
	if ErrInvalidArgument != 30 || lastKnownErrorCode != ErrInvalidArgument {
		t.Errorf("ErrInvalidArgument = %d, last known %d; want 30, 30", ErrInvalidArgument, lastKnownErrorCode)
	}
	for code, want := range cases {
		if got := ErrorCodeToString(code); got != want {
			t.Errorf("ErrorCodeToString(%d) = %q, want %q", code, got, want)
		}
		if got := code.String(); got != want {
			t.Errorf("ErrorCode(%d).String() = %q, want %q", code, got, want)
		}
	}
	// A value the library doesn't know must still give a string, not crash.
	if s := ErrorCodeToString(ErrorCode(9999)); s == "" {
		t.Error("ErrorCodeToString(9999) is empty")
	}
}

// TestMain sets a render config for the tests: before SetRenderConfig the renderer has no error policy and
// treats every error as Panic (it aborts the process). SetRenderConfig doesn't open a window, Init does.
func TestMain(m *testing.M) {
	if os.Getenv("NRE_GO_TEST_CHILD") == "before-config" {
		_, err := CreateModel(policeGLB)
		fmt.Printf("CreateModel returned: %v\n", err)
		os.Exit(0)
	}
	if err := SetRenderConfig(64, 64, "nre go test"); err != nil {
		fmt.Fprintln(os.Stderr, "SetRenderConfig:", err)
		os.Exit(1)
	}
	os.Exit(m.Run())
}

const policeGLB = "../../../example/assets/police.glb"

// Errors come back as *Error with the code and the renderer's message, and work with errors.Is/As.
func TestErrors(t *testing.T) {
	_, err := CreateInstance(Model{})
	if !errors.Is(err, ErrInvalidHandle) {
		t.Fatalf("CreateInstance(Model{}): got %v, want ErrInvalidHandle", err)
	}
	var e *Error
	if !errors.As(err, &e) {
		t.Fatalf("error is %T, want *nre.Error", err)
	}
	if e.Code != ErrInvalidHandle || e.Message == "" {
		t.Fatalf("got code %v message %q, want InvalidHandle with a message", e.Code, e.Message)
	}
	var code ErrorCode
	if !errors.As(err, &code) || code != ErrInvalidHandle {
		t.Fatalf("errors.As to ErrorCode gave %v", code)
	}
	if errors.Is(err, ErrFileNotFound) {
		t.Fatal("errors.Is matched the wrong code")
	}
	t.Logf("CreateInstance(Model{}): %v", err)

	// The message is the one libnre_c keeps for this thread (lock it so both calls run on the same one).
	runtime.LockOSThread()
	_, err = CreateInstance(Model{})
	last := GetLastErrorMessage()
	runtime.UnlockOSThread()
	if !errors.As(err, &e) || e.Message != last {
		t.Fatalf("message %q isn't the last error message %q", e.Message, last)
	}

	// TestMain already set a render config.
	if err := SetRenderConfig(64, 64, "again"); !errors.Is(err, ErrAlreadyInitialized) {
		t.Fatalf("second SetRenderConfig: got %v, want ErrAlreadyInitialized", err)
	} else {
		t.Logf("second SetRenderConfig: %v", err)
	}

	// Out-parameter functions report errors too, and strings reach C intact.
	if _, err := GetCameraPosition(); !errors.Is(err, ErrNoSceneBound) {
		t.Fatalf("GetCameraPosition without a scene: got %v, want ErrNoSceneBound", err)
	}
	if _, err := CreateModel("no/such/model.glb"); !errors.Is(err, ErrFileNotFound) ||
		!strings.Contains(err.Error(), "no/such/model.glb") {
		t.Fatalf("CreateModel of a missing file: got %v, want ErrFileNotFound naming the path", err)
	}
	if _, err := GetModelMaterials(Model{}); !errors.Is(err, ErrInvalidHandle) {
		t.Fatalf("GetModelMaterials(Model{}): got %v, want ErrInvalidHandle", err)
	}
	if _, err := CreateTextureFull("no/such/texture.png", false); !errors.Is(err, ErrFileNotFound) {
		t.Fatalf("CreateTextureFull of a missing file: got %v, want ErrFileNotFound", err)
	}
	if err := SetCastsShadows(Instance{}, false); !errors.Is(err, ErrInvalidHandle) {
		t.Fatalf("SetCastsShadows(Instance{}): got %v, want ErrInvalidHandle", err)
	}
}

func TestSceneLighting(t *testing.T) {
	if SceneLightingSmall != 0 || SceneLightingLandscape != 1 {
		t.Fatalf("SceneLighting values %d, %d; want 0, 1", SceneLightingSmall, SceneLightingLandscape)
	}
	if SceneLightingSmall.String() != "Small" || SceneLightingLandscape.String() != "Landscape" ||
		SceneLighting(7).String() != "SceneLighting(7)" {
		t.Fatalf("SceneLighting strings: %v %v %v", SceneLightingSmall, SceneLightingLandscape, SceneLighting(7))
	}
}

// NotInitialized: calling the API before SetRenderConfig (Debug builds check it). With no context yet
// the renderer's policy is Panic, so it prints the NotInitialized error and aborts the process instead of
// returning it; this runs that in a child process and checks the message.
func TestNotInitialized(t *testing.T) {
	cmd := exec.Command(os.Args[0], "-test.run=^$")
	cmd.Env = append(os.Environ(), "NRE_GO_TEST_CHILD=before-config")
	out, err := cmd.CombinedOutput()
	for _, line := range strings.Split(string(out), "\n") {
		if strings.HasPrefix(line, "[NRE]") || strings.HasPrefix(line, "CreateModel returned") {
			t.Logf("child: %s", line)
		}
	}
	t.Logf("child exit: %v", err)
	if !strings.Contains(string(out), "NotInitialized") {
		t.Fatalf("child didn't report NotInitialized (exit: %v)", err)
	}
	if strings.Contains(string(out), "CreateModel returned:") {
		// Not the behaviour today, but fine if the renderer starts returning the error.
		if !strings.Contains(string(out), "nre: NotInitialized") {
			t.Fatalf("CreateModel returned something other than NotInitialized")
		}
	}
}

func TestUnknownErrorCode(t *testing.T) {
	for _, raw := range []int64{int64(lastKnownErrorCode) + 1, 0x7FFFFFFF} {
		e := rawError(raw, "boom")
		if e.Code != ErrUnknown || !errors.Is(e, ErrUnknown) {
			t.Errorf("raw code %d became %v, want ErrUnknown", raw, e.Code)
		}
	}
	if e := rawError(int64(ErrWrongMode), "m"); e.Code != ErrWrongMode || e.Message != "m" {
		t.Errorf("known code changed: %+v", e)
	}
}

func TestLayout(t *testing.T) {
	c := cLayout()
	want := map[string]uintptr{
		"Vec2": 8, "Vec3": 12, "Quat": 16, "Mat4": 64, "Transform": 40,
		"Transform.position": 0, "Transform.rotation": 12, "Transform.scale": 28,
		"Shader": 8, "Pipeline": 8, "Model": 8, "Instance": 8, "Light": 8,
		"Scene": 8, "Texture": 8, "Material": 8, "Cubemap": 8,
		"ErrorCode": 4, "Key": 4, "SceneLighting": 4,
		"Vertex": 32, "Vertex.position": 0, "Vertex.normal": 12, "Vertex.uv": 24,
		"RayHit": 24, "RayHit.instance": 0, "RayHit.distance": 8, "RayHit.point": 12,
	}
	for name, size := range want {
		if c[name] != size {
			t.Errorf("C %s: %d bytes, want %d", name, c[name], size)
		}
	}
	// The Go types match the C ones too.
	var tr Transform
	var v Vertex
	var r RayHit
	goSizes := map[string]uintptr{
		"Vec2": unsafe.Sizeof(Vec2{}), "Vec3": unsafe.Sizeof(Vec3{}), "Quat": unsafe.Sizeof(Quat{}),
		"Mat4": unsafe.Sizeof(Mat4{}), "Transform": unsafe.Sizeof(tr),
		"Transform.position": unsafe.Offsetof(tr.Position), "Transform.rotation": unsafe.Offsetof(tr.Rotation),
		"Transform.scale": unsafe.Offsetof(tr.Scale),
		"Shader":          unsafe.Sizeof(Shader{}), "Pipeline": unsafe.Sizeof(Pipeline{}), "Model": unsafe.Sizeof(Model{}),
		"Instance": unsafe.Sizeof(Instance{}), "Light": unsafe.Sizeof(Light{}), "Scene": unsafe.Sizeof(Scene{}),
		"Texture": unsafe.Sizeof(Texture{}), "Material": unsafe.Sizeof(Material{}), "Cubemap": unsafe.Sizeof(Cubemap{}),
		"ErrorCode": unsafe.Sizeof(ErrorCode(0)), "Key": unsafe.Sizeof(Key(0)),
		"SceneLighting": unsafe.Sizeof(SceneLighting(0)),
		"Vertex":        unsafe.Sizeof(v), "Vertex.position": unsafe.Offsetof(v.Position),
		"Vertex.normal": unsafe.Offsetof(v.Normal), "Vertex.uv": unsafe.Offsetof(v.UV),
		"RayHit": unsafe.Sizeof(r), "RayHit.instance": unsafe.Offsetof(r.Instance),
		"RayHit.distance": unsafe.Offsetof(r.Distance), "RayHit.point": unsafe.Offsetof(r.Point),
	}
	for name, size := range goSizes {
		if c[name] != size {
			t.Errorf("Go %s: %d bytes, C has %d", name, size, c[name])
		}
	}
}

func TestConversions(t *testing.T) {
	tr := Transform{Position: Vec3{1, 2, 3}, Rotation: Quat{0.1, 0.2, 0.3, 0.9}, Scale: Vec3{4, 5, 6}}
	h := Model{index: 7, generation: 42}
	gotT, gotH := roundTrip(tr, h)
	if gotT != tr || gotH != h {
		t.Fatalf("round trip: got %+v %+v, want %+v %+v", gotT, gotH, tr, h)
	}
	hit := RayHit{Instance: Instance{index: 3, generation: 9}, Distance: 2.5, Point: Vec3{0, 0, 0.5}}
	if got := rayHitRoundTrip(hit); got != hit {
		t.Fatalf("RayHit round trip: got %+v, want %+v", got, hit)
	}
	if (Texture{}) != (Texture{index: 0, generation: 0}) {
		t.Fatal("zero Texture isn't the empty handle")
	}
	id := IdentityTransform()
	if id.Rotation != (Quat{0, 0, 0, 1}) || id.Scale != (Vec3{1, 1, 1}) || id.Position != (Vec3{}) {
		t.Fatalf("IdentityTransform = %+v", id)
	}
	if v := (Vec3{1, 2, 3}).Add(Vec3{1, 1, 1}).Sub(Vec3{0, 1, 0}).Scale(2); v != (Vec3{4, 4, 8}) {
		t.Fatalf("Vec3 math = %+v", v)
	}
}
