//! Conversions to and from other math libraries, behind the `glam` and `mint` features (off by default).
//! Both directions, field by field. Vector parameters take `impl Into<Vec3>`, so with a feature on, a
//! `glam::Vec3` (or `mint::Vector3<f32>`) can be passed directly; returned values convert with `.into()`.
//! Matrices are column-major on both sides.

#[cfg(feature = "glam")]
mod glam_conversions {
    use crate::{Mat4, Quat, Vec2, Vec3};

    impl From<glam::Vec2> for Vec2 {
        fn from(v: glam::Vec2) -> Self {
            Self::new(v.x, v.y)
        }
    }
    impl From<Vec2> for glam::Vec2 {
        fn from(v: Vec2) -> Self {
            Self::new(v.x, v.y)
        }
    }
    impl From<glam::Vec3> for Vec3 {
        fn from(v: glam::Vec3) -> Self {
            Self::new(v.x, v.y, v.z)
        }
    }
    impl From<Vec3> for glam::Vec3 {
        fn from(v: Vec3) -> Self {
            Self::new(v.x, v.y, v.z)
        }
    }
    impl From<glam::Vec3A> for Vec3 {
        fn from(v: glam::Vec3A) -> Self {
            Self::new(v.x, v.y, v.z)
        }
    }
    impl From<Vec3> for glam::Vec3A {
        fn from(v: Vec3) -> Self {
            Self::new(v.x, v.y, v.z)
        }
    }
    impl From<glam::Quat> for Quat {
        fn from(q: glam::Quat) -> Self {
            Self::new(q.x, q.y, q.z, q.w)
        }
    }
    impl From<Quat> for glam::Quat {
        fn from(q: Quat) -> Self {
            Self::from_xyzw(q.x, q.y, q.z, q.w)
        }
    }
    impl From<glam::Mat4> for Mat4 {
        fn from(m: glam::Mat4) -> Self {
            Self { m: m.to_cols_array() }
        }
    }
    impl From<Mat4> for glam::Mat4 {
        fn from(m: Mat4) -> Self {
            Self::from_cols_array(&m.m)
        }
    }
}

#[cfg(feature = "mint")]
mod mint_conversions {
    use crate::{Mat4, Quat, Vec2, Vec3};

    impl From<mint::Vector2<f32>> for Vec2 {
        fn from(v: mint::Vector2<f32>) -> Self {
            Self::new(v.x, v.y)
        }
    }
    impl From<Vec2> for mint::Vector2<f32> {
        fn from(v: Vec2) -> Self {
            Self { x: v.x, y: v.y }
        }
    }
    impl From<mint::Point2<f32>> for Vec2 {
        fn from(p: mint::Point2<f32>) -> Self {
            Self::new(p.x, p.y)
        }
    }
    impl From<Vec2> for mint::Point2<f32> {
        fn from(v: Vec2) -> Self {
            Self { x: v.x, y: v.y }
        }
    }
    impl From<mint::Vector3<f32>> for Vec3 {
        fn from(v: mint::Vector3<f32>) -> Self {
            Self::new(v.x, v.y, v.z)
        }
    }
    impl From<Vec3> for mint::Vector3<f32> {
        fn from(v: Vec3) -> Self {
            Self { x: v.x, y: v.y, z: v.z }
        }
    }
    impl From<mint::Point3<f32>> for Vec3 {
        fn from(p: mint::Point3<f32>) -> Self {
            Self::new(p.x, p.y, p.z)
        }
    }
    impl From<Vec3> for mint::Point3<f32> {
        fn from(v: Vec3) -> Self {
            Self { x: v.x, y: v.y, z: v.z }
        }
    }
    impl From<mint::Quaternion<f32>> for Quat {
        fn from(q: mint::Quaternion<f32>) -> Self {
            Self::new(q.v.x, q.v.y, q.v.z, q.s)
        }
    }
    impl From<Quat> for mint::Quaternion<f32> {
        fn from(q: Quat) -> Self {
            Self { v: mint::Vector3 { x: q.x, y: q.y, z: q.z }, s: q.w }
        }
    }
    /// Column-major on both sides: `m.x` is column 0.
    impl From<mint::ColumnMatrix4<f32>> for Mat4 {
        fn from(m: mint::ColumnMatrix4<f32>) -> Self {
            let columns = [m.x, m.y, m.z, m.w];
            Self { m: std::array::from_fn(|i| {
                let c = columns[i / 4];
                [c.x, c.y, c.z, c.w][i % 4]
            }) }
        }
    }
    impl From<Mat4> for mint::ColumnMatrix4<f32> {
        fn from(m: Mat4) -> Self {
            let column = |c: usize| mint::Vector4 { x: m.get(c, 0), y: m.get(c, 1), z: m.get(c, 2), w: m.get(c, 3) };
            Self { x: column(0), y: column(1), z: column(2), w: column(3) }
        }
    }
}
