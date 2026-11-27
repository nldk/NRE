//! Checks that need no window or GPU: run with `cargo test` (NRE_LIB_DIR pointing at libnre_c).

use std::mem::{align_of, offset_of, size_of};

use nre::*;

#[test]
fn version_check_passes() {
    assert_eq!(ABI_VERSION, (0, 3), "these bindings are for libnre_c 0.3");
    let (major, minor, _) = get_version();
    assert_eq!((major, minor), (0, 3), "loaded the wrong libnre_c");
    check_version().expect("version check");
}

#[test]
fn error_code_to_string_works() {
    assert_eq!(error_code_to_string(ErrorCode::FileNotFound), "FileNotFound");
    assert_eq!(error_code_to_string(ErrorCode::NotInitialized), "NotInitialized");
    assert_eq!(error_code_to_string(ErrorCode::LibraryLoadFailed), "LibraryLoadFailed");
    assert_eq!(error_code_to_string(ErrorCode::InvalidArgument), "InvalidArgument");
    assert_eq!(ErrorCode::WrongMode.to_string(), "WrongMode");
}

#[test]
fn error_codes_map_safely() {
    assert_eq!(ErrorCode::from_raw(sys::NRE_ERROR_FILE_NOT_FOUND), Some(ErrorCode::FileNotFound));
    assert_eq!(ErrorCode::FileNotFound.to_raw(), 7);
    assert_eq!(ErrorCode::from_raw(sys::NRE_OK), None);
    assert_eq!(ErrorCode::from_raw(12345), None);
    assert_eq!(ErrorCode::from_raw(-1), None);
    assert_eq!(Key::from_raw(sys::NRE_KEY_W), Some(Key::W));
    assert_eq!(Key::KeypadEqual.to_raw(), 336);
    assert_eq!(ErrorCode::InvalidArgument.to_raw(), 30);
    assert_eq!(ErrorCode::from_raw(sys::NRE_ERROR_INVALID_ARGUMENT), Some(ErrorCode::InvalidArgument));
}

#[test]
fn scene_lighting_values() {
    assert_eq!(SceneLighting::Small.to_raw(), 0);
    assert_eq!(SceneLighting::Landscape.to_raw(), 1);
    assert_eq!(SceneLighting::from_raw(sys::NRE_SCENE_LIGHTING_LANDSCAPE), Some(SceneLighting::Landscape));
    assert_eq!(SceneLighting::from_raw(2), None);
    assert_eq!(SceneLighting::from_raw(sys::NRE_SCENE_LIGHTING_MAX_ENUM), None);
    assert_eq!(SceneLighting::default(), SceneLighting::Small);
}

// The brief asked for "create_model before init returns NotInitialized", but the renderer can't do that
// without crashing the test process (see README.md, "Known renderer issues"): before set_render_config there
// is no context and every error is treated as Panic (abort), and between set_render_config and init,
// create_model skips the init check and asserts inside VMA. So this checks the same error path (code +
// message read right after the failing call) with a call that fails cleanly after set_render_config.
#[test]
fn failing_call_returns_code_and_message() {
    // Doesn't open the window (init does). The default error policy is Return.
    set_render_config(100, 100, "headless test").expect("set_render_config");
    let error = get_instance_transform(Instance::NONE).expect_err("the empty handle must be rejected");
    assert_eq!(error.code, ErrorCode::InvalidHandle);
    assert!(!error.message.is_empty(), "the error should carry the library's message");
    assert_eq!(get_last_error_message(), error.message);
    println!("get_instance_transform(Instance::NONE): {error}");

    let error = set_render_config(100, 100, "again").expect_err("a second set_render_config must fail");
    assert_eq!(error.code, ErrorCode::AlreadyInitialized);
    assert!(!error.message.is_empty());
    println!("second set_render_config: {error}");
}

// Misuse before set_render_config aborts the process (renderer.h's Panic policy, since there's no context yet),
// so check it in a child process: this test binary, running only the ignored test below.
#[test]
fn create_model_before_set_render_config_aborts() {
    let output = std::process::Command::new(std::env::current_exe().unwrap())
        .args(["--exact", "child_create_model_before_set_render_config", "--ignored", "--nocapture"])
        .env("NRE_TEST_CHILD", "1")
        .output()
        .expect("run the child test");
    let stderr = String::from_utf8_lossy(&output.stderr);
    assert!(!output.status.success(), "the child should have aborted");
    assert!(stderr.contains("NotInitialized"), "child stderr: {stderr}");
    println!("child: {:?}, stderr: {}", output.status, stderr.trim());
}

#[test]
#[ignore = "run by create_model_before_set_render_config_aborts in a child process"]
fn child_create_model_before_set_render_config() {
    if std::env::var_os("NRE_TEST_CHILD").is_some() {
        let _ = create_model("police.glb");
    }
}

#[test]
fn interior_nul_is_an_error_not_a_panic() {
    let error = create_model("bad\0path.glb").expect_err("a NUL byte must be rejected");
    assert_eq!(error.code, ErrorCode::Unknown);
    let error = set_render_config(100, 100, "bad\0title").expect_err("a NUL byte must be rejected");
    assert_eq!(error.code, ErrorCode::Unknown);
}

#[test]
fn layouts_match_nre_h() {
    assert_eq!(size_of::<Vec2>(), 8);
    assert_eq!(size_of::<Vec3>(), 12);
    assert_eq!(size_of::<Quat>(), 16);
    assert_eq!(size_of::<Mat4>(), 64);
    assert_eq!(size_of::<Transform>(), 40);
    assert_eq!(align_of::<Transform>(), 4);
    assert_eq!(offset_of!(Transform, position), 0);
    assert_eq!(offset_of!(Transform, rotation), 12);
    assert_eq!(offset_of!(Transform, scale), 28);
    assert_eq!(size_of::<sys::NreTransform>(), 40);
    assert_eq!(offset_of!(sys::NreTransform, rotation), 12);
    assert_eq!(offset_of!(sys::NreTransform, scale), 28);
    assert_eq!(size_of::<sys::NreMat4>(), 64);
    assert_eq!(size_of::<sys::NreQuat>(), 16);

    // Vertex slices go to C as NreVertex arrays: same layout.
    assert_eq!((size_of::<Vertex>(), align_of::<Vertex>()), (32, 4));
    assert_eq!((offset_of!(Vertex, position), offset_of!(Vertex, normal), offset_of!(Vertex, uv)), (0, 12, 24));
    assert_eq!(size_of::<sys::NreVertex>(), 32);
    assert_eq!((offset_of!(sys::NreVertex, normal), offset_of!(sys::NreVertex, uv)), (12, 24));
    assert_eq!(size_of::<sys::NreRayHit>(), 24);
    assert_eq!((offset_of!(sys::NreRayHit, instance), offset_of!(sys::NreRayHit, distance)), (0, 8));
    assert_eq!(offset_of!(sys::NreRayHit, point), 12);
    assert_eq!(size_of::<RayHit>(), 24);
    assert_eq!((offset_of!(RayHit, distance), offset_of!(RayHit, point)), (8, 12));

    let handles = [
        ("Shader", size_of::<Shader>(), size_of::<sys::NreShader>()),
        ("Pipeline", size_of::<Pipeline>(), size_of::<sys::NrePipeline>()),
        ("Model", size_of::<Model>(), size_of::<sys::NreModel>()),
        ("Instance", size_of::<Instance>(), size_of::<sys::NreInstance>()),
        ("Light", size_of::<Light>(), size_of::<sys::NreLight>()),
        ("Scene", size_of::<Scene>(), size_of::<sys::NreScene>()),
        ("Texture", size_of::<Texture>(), size_of::<sys::NreTexture>()),
        ("Material", size_of::<Material>(), size_of::<sys::NreMaterial>()),
        ("Cubemap", size_of::<Cubemap>(), size_of::<sys::NreCubemap>()),
    ];
    for (name, safe, raw) in handles {
        assert_eq!((safe, raw), (8, 8), "{name}");
    }
}

#[test]
fn empty_handles_and_math_defaults() {
    assert_eq!(Model::NONE.to_raw(), sys::NreModel { index: 0, generation: 0 });
    assert_eq!(Texture::default(), Texture::NONE);
    assert_eq!(Transform::default(), Transform::IDENTITY);
    assert_eq!(Transform::IDENTITY.rotation, Quat::new(0.0, 0.0, 0.0, 1.0));
    assert_eq!(Transform::IDENTITY.scale, Vec3::ONE);
    assert_eq!(Vec3::from([1.0, 2.0, 3.0]), Vec3::from((1.0, 2.0, 3.0)));
    assert_eq!(Mat4::from([[1.0, 2.0, 3.0, 4.0], [0.0; 4], [0.0; 4], [0.0; 4]]).get(0, 3), 4.0);
    assert_eq!(Vertex::default(), Vertex::new(Vec3::ZERO, Vec3::ZERO, Vec2::ZERO));
    assert_eq!(Vertex::from_position(Vec3::X).normal, Vec3::ZERO);
}
