//! Port of example/main.cpp: the police car on the plains skybox, slowly rotating, with an orbit camera
//! (drag with the left mouse button to orbit, scroll to zoom). Switch API_MODE to try both API modes.
//!
//!     NRE_LIB_DIR=/path/to/cmake-build-debug/bin cargo run --example orbit
//!
//! Assets come from <repo>/example/assets; shaders from $NRE_SHADER_DIR, defaulting to
//! <repo>/cmake-build-debug/example/shaders.

use std::env;
use std::path::{Path, PathBuf};

use nre::{ApiMode, MouseButton, RenderErrorPolicy, RenderMode, ShaderType, Vec3};

const API_MODE: ApiMode = ApiMode::FunctionDraw; // or ApiMode::Scene

/// Lives in the app, not the library: the library only provides the camera primitives it uses.
struct OrbitCamera {
    target: Vec3,
    distance: f32,
    yaw: f32,
    pitch: f32,
}

impl OrbitCamera {
    const ROTATE_SPEED: f32 = 0.01;
    const ZOOM_STEP: f32 = 0.9;
    const MAX_PITCH: f32 = 1.55;

    fn new(target: Vec3) -> Self {
        Self { target, distance: 3.0, yaw: 0.0, pitch: 0.0 }
    }

    fn update(&mut self) -> nre::Result<()> {
        if nre::is_mouse_button_down(MouseButton::Left)? {
            let d = nre::get_mouse_delta()?;
            self.yaw -= d.x * Self::ROTATE_SPEED;
            self.pitch = (self.pitch + d.y * Self::ROTATE_SPEED).clamp(-Self::MAX_PITCH, Self::MAX_PITCH);
        }

        self.distance = (self.distance * Self::ZOOM_STEP.powf(nre::get_scroll_delta()?)).max(0.1);

        let (sin_yaw, cos_yaw) = self.yaw.sin_cos();
        let (sin_pitch, cos_pitch) = self.pitch.sin_cos();
        let offset = Vec3::new(cos_pitch * sin_yaw, sin_pitch, cos_pitch * cos_yaw);
        nre::set_camera_position(self.target + offset * self.distance)?;
        nre::camera_look_at(self.target)
    }
}

fn main() -> nre::Result<()> {
    // <repo> from this crate's location (bindings/rust/nre), not the working directory.
    let repo = Path::new(env!("CARGO_MANIFEST_DIR")).join("../../..");
    let assets = repo.join("example/assets");
    let shaders = env::var_os("NRE_SHADER_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| repo.join("cmake-build-debug/example/shaders"));

    nre::set_render_config_with(
        500,
        500,
        "test",
        RenderMode::Render3D,
        API_MODE,
        RenderErrorPolicy::Ignore,
        false,
        true,
        true,
    )?;
    nre::init()?;
    let model = nre::create_model(assets.join("police.glb"))?;
    let car = nre::create_instance(model)?;
    let vertex = nre::create_shader(shaders.join("vertex.vert.spv"), ShaderType::Vertex)?;
    let fragment = nre::create_shader(shaders.join("fragment.frag.spv"), ShaderType::Fragment)?;
    let pipeline = nre::create_pipeline(vertex, fragment)?;
    nre::bind_pipeline(pipeline)?;
    nre::create_scene()?;
    nre::add_instance_to_scene(car)?;
    nre::translate_instance(car, [0.0, 0.0, -1.0])?;
    let sky = assets.join("skyboxes/plains");
    let sky = nre::create_cubemap(
        sky.join("right.png"),
        sky.join("left.png"),
        sky.join("top.png"),
        sky.join("bottom.png"),
        sky.join("front.png"),
        sky.join("back.png"),
    )?;
    nre::set_skybox(Some(sky))?;

    let mut camera = OrbitCamera::new(nre::get_instance_transform(car)?.position);
    while !nre::window_should_close()? {
        nre::poll_events()?;
        camera.update()?;
        match API_MODE {
            ApiMode::FunctionDraw => {
                nre::render_frame()?;
                nre::draw_instance(car)?;
                nre::end_frame()?;
            }
            _ => nre::render_frame()?,
        }

        nre::rotate_instance(car, 0.0001, Vec3::Y)?;
    }
    nre::shutdown()
}
