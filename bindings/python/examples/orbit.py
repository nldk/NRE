"""Port of example/main.cpp: the police car on the plains skybox, slowly rotating, with an orbit camera
(drag with the left mouse button to orbit, scroll to zoom).

    NRE_LIB_DIR=<repo>/cmake-build-debug/bin python3 examples/orbit.py

Shaders come from $NRE_SHADER_DIR (default <repo>/cmake-build-debug/example/shaders), assets from
<repo>/example/assets. Set API_MODE to switch between SceneMode and FunctionDrawMode.
"""

from __future__ import annotations

import math
import os
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # run from the source tree without installing

import nre  # noqa: E402

ASSET_DIR = REPO / "example" / "assets"
SHADER_DIR = Path(os.environ.get("NRE_SHADER_DIR", REPO / "cmake-build-debug" / "example" / "shaders"))

API_MODE = nre.ApiMode.FUNCTION_DRAW  # or nre.ApiMode.SCENE


class OrbitCamera:
    ROTATE_SPEED = 0.01
    ZOOM_STEP = 0.9
    MAX_PITCH = 1.55

    def __init__(self, target: nre.Vec3Like = (0.0, 0.0, 0.0)) -> None:
        self.target = nre.Vec3(*target)
        self.distance = 3.0
        self.yaw = 0.0
        self.pitch = 0.0

    def update(self) -> None:
        if nre.is_mouse_button_down(nre.MouseButton.LEFT):
            dx, dy = nre.get_mouse_delta()
            self.yaw -= dx * self.ROTATE_SPEED
            self.pitch = min(max(self.pitch + dy * self.ROTATE_SPEED, -self.MAX_PITCH), self.MAX_PITCH)

        self.distance = max(self.distance * self.ZOOM_STEP ** nre.get_scroll_delta(), 0.1)

        offset = nre.Vec3(math.cos(self.pitch) * math.sin(self.yaw), math.sin(self.pitch),
                          math.cos(self.pitch) * math.cos(self.yaw))
        nre.set_camera_position(self.target + offset * self.distance)
        nre.camera_look_at(self.target)


def main() -> int:
    nre.set_render_config(500, 500, "test", api_mode=API_MODE, error_policy=nre.RenderErrorPolicy.IGNORE)
    nre.init()
    try:
        model = nre.create_model(ASSET_DIR / "police.glb")
        car = nre.create_instance(model)
        vertex = nre.create_shader(SHADER_DIR / "vertex.vert.spv", nre.ShaderType.VERTEX)
        frag = nre.create_shader(SHADER_DIR / "fragment.frag.spv", nre.ShaderType.FRAGMENT)
        pipeline = nre.create_pipeline(vertex, frag)
        nre.bind_pipeline(pipeline)
        nre.create_scene()
        nre.add_instance_to_scene(car)
        nre.translate_instance(car, (0.0, 0.0, -1.0))
        sky_dir = ASSET_DIR / "skyboxes" / "plains"
        sky = nre.create_cubemap(*(sky_dir / f"{face}.png"
                                   for face in ("right", "left", "top", "bottom", "front", "back")))
        nre.set_skybox(sky)

        camera = OrbitCamera(nre.get_instance_transform(car).position)
        while not nre.window_should_close():
            nre.poll_events()
            camera.update()
            if API_MODE == nre.ApiMode.SCENE:
                nre.render_frame()
            else:
                nre.render_frame()
                nre.draw_instance(car)
                nre.end_frame()

            nre.rotate_instance(car, 0.0001, (0.0, 1.0, 0.0))
    finally:
        nre.shutdown()
    return 0


if __name__ == "__main__":
    sys.exit(main())
