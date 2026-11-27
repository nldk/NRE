// Port of example/main.cpp: the police car on the plains skybox, slowly rotating, with an orbit camera
// (drag with the left mouse button to orbit, scroll to zoom). Run with bindings/odin/run.sh orbit.
package orbit

import "core:fmt"
import "core:math"
import "core:os"
import nre "../../nre"

// <repo>, found from this file's directory (bindings/odin/examples/orbit), not the working directory.
REPO_DIR :: #directory + "/../../../.."
ASSET_DIR :: REPO_DIR + "/example/assets"
DEFAULT_SHADER_DIR :: REPO_DIR + "/cmake-build-debug/example/shaders"

API_MODE :: nre.Api_Mode.Function_Draw // or .Scene

// The camera controller lives in the app: the bindings only give the primitives.
Orbit_Camera :: struct {
	target:   nre.Vec3,
	distance: f32,
	yaw:      f32,
	pitch:    f32,
}

ROTATE_SPEED :: 0.01
ZOOM_STEP :: 0.9
MAX_PITCH :: 1.55

orbit_camera_update :: proc(cam: ^Orbit_Camera) -> (err: nre.Error_Code) {
	if nre.is_mouse_button_down(.Left) or_return {
		d := nre.get_mouse_delta() or_return
		cam.yaw -= d.x * ROTATE_SPEED
		cam.pitch = clamp(cam.pitch + d.y * ROTATE_SPEED, -MAX_PITCH, MAX_PITCH)
	}

	scroll := nre.get_scroll_delta() or_return
	cam.distance = max(cam.distance * math.pow(f32(ZOOM_STEP), scroll), 0.1)

	offset := nre.Vec3{
		math.cos(cam.pitch) * math.sin(cam.yaw),
		math.sin(cam.pitch),
		math.cos(cam.pitch) * math.cos(cam.yaw),
	}
	nre.set_camera_position(cam.target + offset * cam.distance) or_return
	nre.camera_look_at(cam.target) or_return
	return .Ok
}

run :: proc() -> (err: nre.Error_Code) {
	shader_dir, found := os.lookup_env("NRE_SHADER_DIR", context.allocator)
	if !found do shader_dir = DEFAULT_SHADER_DIR

	nre.set_render_config(500, 500, "test", api_mode = API_MODE, error_policy = .Ignore) or_return
	nre.init() or_return
	defer nre.shutdown()

	model := nre.create_model(ASSET_DIR + "/police.glb") or_return
	car := nre.create_instance(model) or_return
	vertex := nre.create_shader(fmt.tprintf("%s/vertex.vert.spv", shader_dir), .Vertex) or_return
	frag := nre.create_shader(fmt.tprintf("%s/fragment.frag.spv", shader_dir), .Fragment) or_return
	pipeline := nre.create_pipeline(vertex, frag) or_return
	nre.bind_pipeline(pipeline) or_return
	_ = nre.create_scene() or_return
	nre.add_instance_to_scene(car) or_return
	nre.translate_instance(car, {0, 0, -1}) or_return

	SKY :: ASSET_DIR + "/skyboxes/plains/"
	sky := nre.create_cubemap(SKY + "right.png", SKY + "left.png", SKY + "top.png",
	                          SKY + "bottom.png", SKY + "front.png", SKY + "back.png") or_return
	nre.set_skybox(sky) or_return

	camera := Orbit_Camera{distance = 3}
	camera.target = (nre.get_instance_transform(car) or_return).position

	for !(nre.window_should_close() or_return) {
		nre.poll_events() or_return
		orbit_camera_update(&camera) or_return
		when API_MODE == .Scene {
			nre.render_frame() or_return
		} else {
			nre.render_frame() or_return
			nre.draw_instance(car) or_return
			nre.end_frame() or_return
		}
		nre.rotate_instance(car, 0.0001, {0, 1, 0}) or_return
		free_all(context.temp_allocator)
	}
	return .Ok
}

main :: proc() {
	if err := run(); err != .Ok {
		fmt.eprintfln("nre error %v: %s", err, nre.last_error_message())
		os.exit(1)
	}
}
