//! The `glam` / `mint` conversions: `cargo test --features glam,mint`. Headless, no library calls.
#![cfg(any(feature = "glam", feature = "mint"))]

#[cfg(feature = "glam")]
#[test]
fn glam_round_trips() {
    let v = nre::Vec3::new(1.0, 2.0, 3.0);
    let g: glam::Vec3 = v.into();
    assert_eq!(g, glam::Vec3::new(1.0, 2.0, 3.0));
    assert_eq!(nre::Vec3::from(g), v);
    assert_eq!(nre::Vec3::from(glam::Vec3A::new(1.0, 2.0, 3.0)), v);
    assert_eq!(glam::Vec3A::from(v), glam::Vec3A::new(1.0, 2.0, 3.0));

    assert_eq!(glam::Vec2::from(nre::Vec2::new(4.0, 5.0)), glam::Vec2::new(4.0, 5.0));
    assert_eq!(nre::Vec2::from(glam::Vec2::new(4.0, 5.0)), nre::Vec2::new(4.0, 5.0));

    let q = glam::Quat::from_axis_angle(glam::Vec3::Y, 0.5);
    let n: nre::Quat = q.into();
    assert_eq!(n, nre::Quat::new(q.x, q.y, q.z, q.w));
    assert_eq!(glam::Quat::from(n), q);

    // Column-major on both sides: the translation is column 3.
    let m = glam::Mat4::from_translation(glam::Vec3::new(7.0, 8.0, 9.0));
    let n: nre::Mat4 = m.into();
    assert_eq!((n.get(3, 0), n.get(3, 1), n.get(3, 2), n.get(3, 3)), (7.0, 8.0, 9.0, 1.0));
    assert_eq!(glam::Mat4::from(n), m);
}

/// Vector parameters take `impl Into<Vec3>`, so glam values go straight in.
#[cfg(feature = "glam")]
#[test]
fn glam_values_fit_vector_parameters() {
    fn takes_vec3(v: impl Into<nre::Vec3>) -> nre::Vec3 {
        v.into()
    }
    assert_eq!(takes_vec3(glam::Vec3::X), nre::Vec3::X);
    // The typed layer's methods have the same signatures; this only needs to compile.
    let _ = |nre: &nre::Context| nre.set_camera_position(glam::Vec3::new(0.0, 1.0, 3.0));
}

#[cfg(feature = "mint")]
#[test]
fn mint_round_trips() {
    let v = nre::Vec3::new(1.0, 2.0, 3.0);
    let m: mint::Vector3<f32> = v.into();
    assert_eq!(m, mint::Vector3 { x: 1.0, y: 2.0, z: 3.0 });
    assert_eq!(nre::Vec3::from(m), v);
    assert_eq!(nre::Vec3::from(mint::Point3 { x: 1.0, y: 2.0, z: 3.0 }), v);
    assert_eq!(mint::Point3::from(v), mint::Point3 { x: 1.0, y: 2.0, z: 3.0 });

    let v2 = nre::Vec2::new(4.0, 5.0);
    assert_eq!(nre::Vec2::from(mint::Vector2::from(v2)), v2);
    assert_eq!(nre::Vec2::from(mint::Point2::from(v2)), v2);

    let q = nre::Quat::new(0.1, 0.2, 0.3, 0.9);
    let mq: mint::Quaternion<f32> = q.into();
    assert_eq!((mq.v.x, mq.v.y, mq.v.z, mq.s), (0.1, 0.2, 0.3, 0.9));
    assert_eq!(nre::Quat::from(mq), q);

    let n = nre::Mat4::from(std::array::from_fn::<f32, 16, _>(|i| i as f32));
    let mm: mint::ColumnMatrix4<f32> = n.into();
    assert_eq!((mm.x.x, mm.x.w, mm.y.x, mm.w.w), (0.0, 3.0, 4.0, 15.0)); // columns of four
    assert_eq!(nre::Mat4::from(mm), n);
}

#[cfg(all(feature = "glam", feature = "mint"))]
#[test]
fn glam_and_mint_agree() {
    let m = glam::Mat4::from_rotation_z(0.3) * glam::Mat4::from_translation(glam::Vec3::new(1.0, 2.0, 3.0));
    let via_mint: mint::ColumnMatrix4<f32> = nre::Mat4::from(m).into();
    assert_eq!(nre::Mat4::from(via_mint), nre::Mat4::from(m));
}
