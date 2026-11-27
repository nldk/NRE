//! orbit.rs with the typed layer: the police car on the plains skybox, slowly rotating, with an orbit camera
//! (drag with the left mouse button to orbit, scroll to zoom). Draw mode like example/main.cpp; set
//! NRE_API_MODE=scene for Scene mode.
//!
//!     NRE_LIB_DIR=/path/to/cmake-build-debug/bin cargo run --example orbit_typed
//!
//! Assets come from <repo>/example/assets; shaders from $NRE_SHADER_DIR, defaulting to
//! <repo>/cmake-build-debug/example/shaders.

use std::env;
use std::path::{Path, PathBuf};

use nre::prelude::*;

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

    /// Takes the Context, so it works with a Renderer or (if you move the call) inside a Frame.
    fn update(&mut self, nre: &Context) -> nre::Result<()> {
        if nre.is_mouse_button_down(MouseButton::Left)? {
            let d = nre.get_mouse_delta()?;
            self.yaw -= d.x * Self::ROTATE_SPEED;
            self.pitch = (self.pitch + d.y * Self::ROTATE_SPEED).clamp(-Self::MAX_PITCH, Self::MAX_PITCH);
        }

        self.distance = (self.distance * Self::ZOOM_STEP.powf(nre.get_scroll_delta()?)).max(0.1);

        let (sin_yaw, cos_yaw) = self.yaw.sin_cos();
        let (sin_pitch, cos_pitch) = self.pitch.sin_cos();
        let offset = Vec3::new(cos_pitch * sin_yaw, sin_pitch, cos_pitch * cos_yaw);
        nre.set_camera_position(self.target + offset * self.distance)?;
        nre.camera_look_at(self.target)
    }
}

/// The demo for either mode. `draw` is the mode-specific part of a frame: in Draw mode it draws the car, in
/// Scene mode there's nothing to do (and `Frame<SceneMode>` has no draw calls to make).
fn run<M: Mode>(draw: impl Fn(&Frame<'_, M>, Instance) -> nre::Result<()>) -> nre::Result<()> {
    // <repo> from this crate's location (bindings/rust/nre), not the working directory.
    let repo = Path::new(env!("CARGO_MANIFEST_DIR")).join("../../..");
    let assets = repo.join("example/assets");
    let shaders = env::var_os("NRE_SHADER_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| repo.join("cmake-build-debug/example/shaders"));

    let mut nre = Renderer::<M>::new(500, 500, "test")?;
    let car = nre.create_instance(nre.create_model(assets.join("police.glb"))?)?;
    let vertex = nre.create_shader(shaders.join("vertex.vert.spv"), ShaderType::Vertex)?;
    let fragment = nre.create_shader(shaders.join("fragment.frag.spv"), ShaderType::Fragment)?;
    nre.bind_pipeline(nre.create_pipeline(vertex, fragment)?)?;
    nre.create_scene()?;
    nre.add_instance_to_scene(car)?;
    nre.translate_instance(car, [0.0, 0.0, -1.0])?;
    let sky = assets.join("skyboxes/plains");
    let sky = nre.create_cubemap(
        sky.join("right.png"),
        sky.join("left.png"),
        sky.join("top.png"),
        sky.join("bottom.png"),
        sky.join("front.png"),
        sky.join("back.png"),
    )?;
    nre.set_skybox(Some(sky))?;

    let mut camera = OrbitCamera::new(nre.get_instance_transform(car)?.position);
    while !nre.window_should_close()? {
        nre.poll_events()?;
        camera.update(&nre)?;
        let frame = nre.frame()?;
        draw(&frame, car)?;
        frame.end()?;

        nre.rotate_instance(car, 0.0001, Vec3::Y)?;
    }
    nre.shutdown()
}

fn main() -> nre::Result<()> {
    match env::var("NRE_API_MODE").as_deref() {
        Ok("scene") => run::<SceneMode>(|_, _| Ok(())),
        _ => run::<DrawMode>(|frame, car| frame.draw_instance(car)),
    }
}
