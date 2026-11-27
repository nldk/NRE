//! Typed-layer checks that open a window (and need a GPU and a display), so they're ignored by default. Run them
//! with:
//!
//!     NRE_LIB_DIR=... cargo test --test window -- --ignored --test-threads=1 --nocapture
//!
//! One test, run in phases: the renderer is global, so they can't run in parallel anyway.

use std::path::{Path, PathBuf};

use nre::prelude::*;

const FRAMES: u32 = 30;

fn repo() -> PathBuf {
    Path::new(env!("CARGO_MANIFEST_DIR")).join("../../..")
}

fn shaders() -> PathBuf {
    std::env::var_os("NRE_SHADER_DIR").map(PathBuf::from).unwrap_or_else(|| repo().join("cmake-build-debug/example/shaders"))
}

fn bind_pipeline(nre: &Context) -> nre::Result<()> {
    let vertex = nre.create_shader(shaders().join("vertex.vert.spv"), ShaderType::Vertex)?;
    let fragment = nre.create_shader(shaders().join("fragment.frag.spv"), ShaderType::Fragment)?;
    nre.bind_pipeline(nre.create_pipeline(vertex, fragment)?)
}

fn draw_mode() -> nre::Result<()> {
    let mut nre = Renderer::<DrawMode>::new(320, 240, "nre typed test: draw")?;

    // The single-instance rule, against a live renderer.
    let error = Renderer::<SceneMode>::new(100, 100, "second").expect_err("only one renderer at a time");
    assert_eq!(error.code, ErrorCode::AlreadyInitialized);

    // No pipeline bound yet: render_frame fails, and the guard must close the half-open frame, or the next
    // render_frame would fail with WrongMode ("the previous frame wasn't ended").
    let error = nre.frame().expect_err("no pipeline bound");
    assert_eq!(error.code, ErrorCode::NoPipelineBound);
    bind_pipeline(&nre)?;

    let model = nre.create_model(repo().join("example/assets/police.glb"))?;
    let car = nre.create_instance(model)?;
    for i in 0..FRAMES {
        nre.poll_events()?;
        let frame = nre.frame()?;
        frame.draw_instance(car)?;
        // Context methods during the frame: moving the instance between draws, the camera for the next frame.
        frame.translate_instance(car, [0.5, 0.0, 0.0])?;
        frame.draw_instance(car)?;
        frame.translate_instance(car, [-0.5, 0.0, 0.0])?;
        frame.draw_model(model, Some(&Transform::from_position(Vec3::new(0.0, 1.0, 0.0))))?;
        frame.set_camera_position([0.0, 0.5, 3.0 + i as f32 * 0.01])?;
        frame.camera_look_at(Vec3::ZERO)?;
        if i % 2 == 0 { frame.end()? } else { drop(frame) }
    }
    assert!(nre.get_delta_time()? > 0.0, "frames were presented");

    // Owned handles across frames, through split().
    let (ctx, mut frames) = nre.split();
    let owned_car = ctx.own(ctx.create_instance(model)?);
    for _ in 0..3 {
        ctx.poll_events()?;
        let frame = frames.frame()?;
        frame.draw_instance(*owned_car)?;
    }
    let raw = owned_car.handle();
    drop(owned_car);
    assert_eq!(ctx.get_instance_transform(raw).unwrap_err().code, ErrorCode::InvalidHandle, "destroyed on drop");
    let released = ctx.own(ctx.create_instance(model)?).release();
    ctx.get_instance_transform(released)?; // release() doesn't destroy
    ctx.own(released).destroy()?;

    nre.shutdown()
}

fn scene_mode() -> nre::Result<()> {
    // After a shutdown, a new renderer can be created (the C side re-initializes).
    let mut nre = Renderer::<SceneMode>::new(320, 240, "nre typed test: scene")?;
    bind_pipeline(&nre)?;
    nre.create_scene()?;
    let car = nre.create_instance(nre.create_model(repo().join("example/assets/police.glb"))?)?;
    nre.add_instance_to_scene(car)?;
    nre.set_camera_position([0.0, 0.5, 3.0])?;
    for _ in 0..FRAMES {
        nre.poll_events()?;
        let frame = nre.frame()?;
        frame.rotate_instance(car, 0.05, Vec3::Y)?;
        frame.end()?;
    }
    assert!(nre.get_delta_time()? > 0.0);
    Ok(()) // dropped: shutdown in Drop
}

/// The 0.3 additions: scene lighting, shadows, haze, exposure, sky colour, the projection, picking, mesh models
/// and non-sRGB textures.
fn lighting_and_picking() -> nre::Result<()> {
    let mut nre = Renderer::<SceneMode>::new_with(
        400,
        400,
        "nre typed test: lighting",
        RenderMode::Render3D,
        RenderErrorPolicy::Return,
        false,
        false,
        false,
    )?;
    bind_pipeline(&nre)?;
    nre.create_scene()?;
    let box_instance = nre.create_instance(nre.create_model(repo().join("example/assets/Box.glb"))?)?;
    nre.add_instance_to_scene(box_instance)?;
    nre.set_camera_position([0.0, 0.0, 3.0])?;
    nre.camera_look_at(Vec3::ZERO)?;

    let data_map = nre.create_texture_with(repo().join("example/assets/skyboxes/plains/top.png"), false)?;
    assert_eq!(nre.get_scene_lighting()?, SceneLighting::Small);
    nre.set_scene_lighting(SceneLighting::Landscape)?;
    assert_eq!(nre.get_scene_lighting()?, SceneLighting::Landscape);
    nre.set_sun([-1.0, -1.0, 0.0])?;
    nre.set_shadow_distance(100.0)?;
    assert_eq!(nre.set_shadow_distance(-1.0).unwrap_err().code, ErrorCode::InvalidArgument);
    nre.set_haze(5000.0)?;
    nre.set_haze_with(5000.0, 200.0)?;
    nre.set_exposure(1.5)?;
    nre.set_sky_color([0.2, 0.3, 0.5])?;
    nre.set_casts_shadows(box_instance, false)?;

    for _ in 0..FRAMES {
        nre.poll_events()?;
        nre.frame()?.end()?;
    }

    // A 60 degree field of view with +Y down: m[5] = -1 / tan(30 degrees). Reverse-Z, infinite far: m[14] = near (0.1).
    let projection = nre.get_projection_matrix()?;
    assert!((projection.m[5] + 1.732).abs() < 0.01, "{projection:?}");
    assert!((projection.m[14] - 0.1).abs() < 1e-4, "{projection:?}");

    let hit = nre.pick_instance([200.0, 200.0])?.expect("the box is in the middle of the window");
    assert_eq!(hit.instance, box_instance);
    assert!((hit.distance - 2.5).abs() < 0.01, "{hit:?}");
    assert!((hit.point - Vec3::new(0.0, 0.0, 0.5)).length() < 0.01, "{hit:?}");
    assert_eq!(nre.pick_instance(Vec2::new(2.0, 2.0))?, None, "the corner misses");

    let triangle = [
        Vertex::from_position(Vec3::new(-1.0, 0.0, 0.0)),
        Vertex::from_position(Vec3::new(1.0, 0.0, 0.0)),
        Vertex::new(Vec3::new(0.0, 1.0, 0.0), Vec3::ZERO, Vec2::new(0.5, 1.0)),
    ];
    let mesh = nre.create_mesh_model(&triangle, &[0, 1, 2], None)?;
    nre.create_instance(mesh)?;
    let material = nre.create_material(None, None, Some(data_map))?;
    nre.create_mesh_model(&triangle, &[0, 1, 2], Some(material))?;
    nre.shutdown()
}

fn manual_presentation<M: Mode>(draw: impl Fn(&Frame<'_, M>, Instance) -> nre::Result<()>) -> nre::Result<()> {
    let mut nre = Renderer::<M>::new_with(
        320,
        240,
        "nre typed test: manual",
        RenderMode::Render3D,
        RenderErrorPolicy::Return,
        true,
        false,
        true,
    )?;
    assert!(nre.manual_frame_presentation_and_submission());
    bind_pipeline(&nre)?;
    if M::API_MODE == nre::ApiMode::Scene {
        nre.create_scene()?;
    }
    let car = nre.create_instance(nre.create_model(repo().join("example/assets/police.glb"))?)?;
    nre.add_instance_to_scene(car)?;
    // More frames than frames in flight: without the guard's submit + present, a fence would never be signaled
    // and this would hang.
    for _ in 0..FRAMES {
        nre.poll_events()?;
        let frame = nre.frame()?;
        draw(&frame, car)?;
        frame.end()?;
    }
    assert!(nre.get_delta_time()? > 0.0);
    nre.shutdown()
}

#[test]
#[ignore = "opens a window: needs a display and a Vulkan GPU"]
fn typed_layer_with_a_window() {
    draw_mode().expect("draw mode");
    scene_mode().expect("scene mode");
    lighting_and_picking().expect("lighting and picking");
    manual_presentation::<DrawMode>(|frame, car| frame.draw_instance(car)).expect("manual, draw mode");
    manual_presentation::<SceneMode>(|_, _| Ok(())).expect("manual, scene mode");
    // And once more, to see the claim was released by the last shutdown.
    drop(Renderer::<DrawMode>::new(100, 100, "last").expect("a renderer after the others"));
}
