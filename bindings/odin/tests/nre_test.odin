// Headless checks: no window, no GPU work. Run with bindings/odin/run.sh test.
package nre_tests

import "core:strings"
import "core:testing"
import nre "../nre"

@(test)
version_matches_abi :: proc(t: ^testing.T) {
	major, minor, _ := nre.get_version()
	testing.expect_value(t, major, u32(nre.ABI_VERSION_MAJOR))
	testing.expect_value(t, minor, u32(nre.ABI_VERSION_MINOR))
	// The bindings target 0.3: a test run that loaded the stale 0.2 library would fail here.
	testing.expect_value(t, nre.ABI_VERSION_MAJOR, 0)
	testing.expect_value(t, nre.ABI_VERSION_MINOR, 3)
	testing.expect_value(t, major, 0)
	testing.expect_value(t, minor, 3)
}

@(test)
error_code_to_string_works :: proc(t: ^testing.T) {
	testing.expect_value(t, nre.error_code_to_string(.Ok), "Ok")
	testing.expect_value(t, nre.error_code_to_string(.File_Not_Found), "FileNotFound")
	testing.expect_value(t, nre.error_code_to_string(.Not_Initialized), "NotInitialized")
	testing.expect_value(t, nre.error_code_to_string(.Library_Load_Failed), "LibraryLoadFailed")
	testing.expect_value(t, nre.error_code_to_string(.Invalid_Argument), "InvalidArgument")
	// A value the library doesn't know still gives a string, not a crash.
	testing.expect(t, len(nre.error_code_to_string(nre.Error_Code(1000))) > 0)
}

@(test)
unknown_codes_map_to_unknown :: proc(t: ^testing.T) {
	testing.expect_value(t, nre.normalize_error_code(nre.Error_Code(1000)), nre.Error_Code.Unknown)
	testing.expect_value(t, nre.normalize_error_code(nre.Error_Code(-1)), nre.Error_Code.Unknown)
	testing.expect_value(t, nre.normalize_error_code(.Invalid_Handle), nre.Error_Code.Invalid_Handle)
	testing.expect_value(t, nre.normalize_error_code(.Ok), nre.Error_Code.Ok)
	testing.expect_value(t, nre.normalize_error_code(.Invalid_Argument), nre.Error_Code.Invalid_Argument)
	testing.expect_value(t, nre.normalize_error_code(nre.Error_Code(31)), nre.Error_Code.Unknown)
}

@(test)
scene_lighting_values :: proc(t: ^testing.T) {
	testing.expect_value(t, int(nre.Scene_Lighting.Small), 0)
	testing.expect_value(t, int(nre.Scene_Lighting.Landscape), 1)
	testing.expect_value(t, size_of(nre.Scene_Lighting), 4)
	// Values the bindings don't know (a newer library) become .Small.
	testing.expect_value(t, nre.normalize_scene_lighting(nre.Scene_Lighting(2)), nre.Scene_Lighting.Small)
	testing.expect_value(t, nre.normalize_scene_lighting(nre.Scene_Lighting(-1)), nre.Scene_Lighting.Small)
	testing.expect_value(t, nre.normalize_scene_lighting(.Landscape), nre.Scene_Lighting.Landscape)
}

// The only test that touches the renderer's global state, so its steps run in order on one thread.
// Calling into the renderer before set_render_config aborts the process (the error policy is Panic until a
// config exists), so this sets a config first, never calls init (no window, no GPU), and never calls shutdown.
@(test)
error_path_after_config :: proc(t: ^testing.T) {
	testing.expect_value(t, nre.set_render_config(320, 240, "nre odin tests", error_policy = .Return), nre.Error_Code.Ok)

	// A missing file: FileNotFound, the empty handle, and a message naming the file.
	model, err := nre.create_model("does-not-exist.glb")
	testing.expect_value(t, err, nre.Error_Code.File_Not_Found)
	testing.expect_value(t, model, nre.Model{})
	testing.expect(t, strings.contains(nre.last_error_message(), "does-not-exist.glb"), "message should name the file")

	// The empty handle is rejected with InvalidHandle and a message.
	testing.expect_value(t, nre.bind_pipeline(nre.Pipeline{}), nre.Error_Code.Invalid_Handle)
	testing.expect(t, len(nre.last_error_message()) > 0, "expected a non-empty error message")
	testing.expect_value(t, nre.translate_instance(nre.Instance{}, {1, 0, 0}), nre.Error_Code.Invalid_Handle)
	_, terr := nre.get_instance_transform(nre.Instance{})
	testing.expect_value(t, terr, nre.Error_Code.Invalid_Handle)

	// A second config is refused.
	testing.expect_value(t, nre.set_render_config(320, 240, "again", error_policy = .Return), nre.Error_Code.Already_Initialized)
	testing.expect(t, strings.contains(nre.last_error_message(), "setRenderConfig"), "message should name the call")

	// or_return hands the first failure up.
	load :: proc() -> (err: nre.Error_Code) {
		model := nre.create_model("x.glb") or_return
		_ = nre.create_instance(model) or_return
		return .Ok
	}
	testing.expect_value(t, load(), nre.Error_Code.File_Not_Found)

	// Picking without a bound scene fails cleanly: no hit, the zero Ray_Hit.
	hit, ok, pick_err := nre.pick_instance({1, 1})
	testing.expect_value(t, pick_err, nre.Error_Code.No_Scene_Bound)
	testing.expect(t, !ok, "no hit on failure")
	testing.expect_value(t, hit, nre.Ray_Hit{})
	testing.expect_value(t, nre.set_casts_shadows(nre.Instance{}, false), nre.Error_Code.Invalid_Handle)
}

@(test)
layouts_match_nre_h :: proc(t: ^testing.T) {
	testing.expect_value(t, size_of(nre.Vec2), 8)
	testing.expect_value(t, size_of(nre.Vec3), 12)
	testing.expect_value(t, size_of(nre.Quat), 16)
	testing.expect_value(t, size_of(nre.Mat4), 64)
	testing.expect_value(t, size_of(nre.Transform), 40)
	testing.expect_value(t, offset_of(nre.Transform, rotation), 12)
	testing.expect_value(t, offset_of(nre.Transform, scale), 28)
	testing.expect_value(t, align_of(nre.Transform), 4)
	testing.expect_value(t, size_of(nre.Shader), 8)
	testing.expect_value(t, size_of(nre.Pipeline), 8)
	testing.expect_value(t, size_of(nre.Model), 8)
	testing.expect_value(t, size_of(nre.Instance), 8)
	testing.expect_value(t, size_of(nre.Light), 8)
	testing.expect_value(t, size_of(nre.Scene), 8)
	testing.expect_value(t, size_of(nre.Texture), 8)
	testing.expect_value(t, size_of(nre.Material), 8)
	testing.expect_value(t, size_of(nre.Cubemap), 8)
	testing.expect_value(t, size_of(nre.Error_Code), 4)
	testing.expect_value(t, size_of(nre.Key), 4)
	testing.expect_value(t, int(nre.Key.Menu), 348)
	testing.expect_value(t, int(nre.Error_Code.Library_Load_Failed), 29)
	testing.expect_value(t, int(nre.Error_Code.Invalid_Argument), 30)
	testing.expect_value(t, size_of(nre.Vertex), 32)
	testing.expect_value(t, align_of(nre.Vertex), 4)
	testing.expect_value(t, offset_of(nre.Vertex, position), 0)
	testing.expect_value(t, offset_of(nre.Vertex, normal), 12)
	testing.expect_value(t, offset_of(nre.Vertex, uv), 24)
	testing.expect_value(t, size_of(nre.Ray_Hit), 24)
	testing.expect_value(t, align_of(nre.Ray_Hit), 4)
	testing.expect_value(t, offset_of(nre.Ray_Hit, instance), 0)
	testing.expect_value(t, offset_of(nre.Ray_Hit, distance), 8)
	testing.expect_value(t, offset_of(nre.Ray_Hit, point), 12)
}

@(test)
math_memory_order_matches_nre_h :: proc(t: ^testing.T) {
	// NreQuat is {x, y, z, w}.
	q: nre.Quat = quaternion(w = 4, x = 1, y = 2, z = 3)
	testing.expect_value(t, transmute([4]f32)q, [4]f32{1, 2, 3, 4})
	testing.expect_value(t, transmute([4]f32)nre.TRANSFORM_IDENTITY.rotation, [4]f32{0, 0, 0, 1})
	// NreMat4 is column-major: m[column * 4 + row]. Odin's m[row, column].
	m: nre.Mat4
	m[1, 3] = 7 // row 1, column 3
	testing.expect_value(t, (transmute([16]f32)m)[3 * 4 + 1], 7)
	// Transform identity and the helper.
	testing.expect_value(t, nre.transform(), nre.TRANSFORM_IDENTITY)
	testing.expect_value(t, nre.transform(position = {1, 2, 3}).scale, nre.Vec3{1, 1, 1})
}
