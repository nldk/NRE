//! Math types with the same layout as nre.h's (and glm's). Angles are in radians; `Mat4` is column-major.
//! Only what's needed to build values and do simple vector math; use your own math library for the rest.

use std::ops::{Add, AddAssign, Mul, Neg, Sub, SubAssign};

use nre_sys::{NreMat4, NreQuat, NreRayHit, NreTransform, NreVec2, NreVec3, NreVertex};

use crate::Instance;

#[repr(C)]
#[derive(Copy, Clone, Debug, Default, PartialEq)]
pub struct Vec2 {
    pub x: f32,
    pub y: f32,
}

#[repr(C)]
#[derive(Copy, Clone, Debug, Default, PartialEq)]
pub struct Vec3 {
    pub x: f32,
    pub y: f32,
    pub z: f32,
}

/// A rotation. Identity: `(0, 0, 0, 1)`.
#[repr(C)]
#[derive(Copy, Clone, Debug, PartialEq)]
pub struct Quat {
    pub x: f32,
    pub y: f32,
    pub z: f32,
    pub w: f32,
}

/// Column-major: `m[column * 4 + row]`, like glm and GLSL.
#[repr(C)]
#[derive(Copy, Clone, Debug, PartialEq)]
pub struct Mat4 {
    pub m: [f32; 16],
}

/// Position/rotation/scale of an object in world space. Applied as scale, then rotation, then translation.
/// Identity (also `Default`): position 0, rotation identity, scale 1.
#[repr(C)]
#[derive(Copy, Clone, Debug, PartialEq)]
pub struct Transform {
    pub position: Vec3,
    pub rotation: Quat,
    pub scale: Vec3,
}

/// One vertex of a model you build yourself with [`crate::create_mesh_model`]: the layout of nre.h's `NreVertex`
/// (and the renderer's own), so a `&[Vertex]` is passed to C as is. A `normal` of zero (also `Default`) gets one
/// computed from the triangles around the vertex, so procedural meshes can skip normals.
#[repr(C)]
#[derive(Copy, Clone, Debug, Default, PartialEq)]
pub struct Vertex {
    pub position: Vec3,
    pub normal: Vec3,
    pub uv: Vec2,
}

/// What [`crate::pick_instance`] hit: the instance, the distance from the camera along the ray (world units) and
/// the world-space point.
#[repr(C)]
#[derive(Copy, Clone, Debug, PartialEq)]
pub struct RayHit {
    pub instance: Instance,
    pub distance: f32,
    pub point: Vec3,
}

// Vertex slices are cast to NreVertex arrays, so the layouts must match exactly.
const _: () = {
    use std::mem::{align_of, offset_of, size_of};
    assert!(size_of::<Vertex>() == size_of::<NreVertex>() && align_of::<Vertex>() == align_of::<NreVertex>());
    assert!(offset_of!(Vertex, position) == offset_of!(NreVertex, position));
    assert!(offset_of!(Vertex, normal) == offset_of!(NreVertex, normal));
    assert!(offset_of!(Vertex, uv) == offset_of!(NreVertex, uv));
};

impl Vertex {
    pub const fn new(position: Vec3, normal: Vec3, uv: Vec2) -> Self {
        Self { position, normal, uv }
    }

    /// A vertex at `position` with a zero normal (computed by [`crate::create_mesh_model`]) and uv `(0, 0)`.
    pub const fn from_position(position: Vec3) -> Self {
        Self::new(position, Vec3::ZERO, Vec2::ZERO)
    }
}

impl Vec2 {
    pub const ZERO: Self = Self::new(0.0, 0.0);

    pub const fn new(x: f32, y: f32) -> Self {
        Self { x, y }
    }
}

impl Vec3 {
    pub const ZERO: Self = Self::splat(0.0);
    pub const ONE: Self = Self::splat(1.0);
    pub const X: Self = Self::new(1.0, 0.0, 0.0);
    pub const Y: Self = Self::new(0.0, 1.0, 0.0);
    pub const Z: Self = Self::new(0.0, 0.0, 1.0);

    pub const fn new(x: f32, y: f32, z: f32) -> Self {
        Self { x, y, z }
    }

    pub const fn splat(v: f32) -> Self {
        Self::new(v, v, v)
    }

    pub fn dot(self, other: Self) -> f32 {
        self.x * other.x + self.y * other.y + self.z * other.z
    }

    pub fn length(self) -> f32 {
        self.dot(self).sqrt()
    }

    /// `self` scaled to length 1 (NaN for the zero vector).
    pub fn normalize(self) -> Self {
        self * (1.0 / self.length())
    }
}

impl Quat {
    pub const IDENTITY: Self = Self::new(0.0, 0.0, 0.0, 1.0);

    pub const fn new(x: f32, y: f32, z: f32, w: f32) -> Self {
        Self { x, y, z, w }
    }

    /// A rotation of `angle` radians around `axis` (normalized here).
    pub fn from_axis_angle(axis: Vec3, angle: f32) -> Self {
        let a = axis.normalize();
        let (s, c) = (angle * 0.5).sin_cos();
        Self::new(a.x * s, a.y * s, a.z * s, c)
    }
}

impl Default for Quat {
    fn default() -> Self {
        Self::IDENTITY
    }
}

impl Mat4 {
    pub const IDENTITY: Self = Self {
        m: [1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0],
    };

    /// The element at `column`, `row`.
    pub const fn get(&self, column: usize, row: usize) -> f32 {
        self.m[column * 4 + row]
    }
}

impl Default for Mat4 {
    fn default() -> Self {
        Self::IDENTITY
    }
}

impl Transform {
    pub const IDENTITY: Self = Self::new(Vec3::ZERO, Quat::IDENTITY, Vec3::ONE);

    pub const fn new(position: Vec3, rotation: Quat, scale: Vec3) -> Self {
        Self { position, rotation, scale }
    }

    /// The identity transform moved to `position`.
    pub const fn from_position(position: Vec3) -> Self {
        Self::new(position, Quat::IDENTITY, Vec3::ONE)
    }
}

impl Default for Transform {
    fn default() -> Self {
        Self::IDENTITY
    }
}

// ----- Building from literals -----
impl From<[f32; 2]> for Vec2 {
    fn from([x, y]: [f32; 2]) -> Self {
        Self::new(x, y)
    }
}
impl From<(f32, f32)> for Vec2 {
    fn from((x, y): (f32, f32)) -> Self {
        Self::new(x, y)
    }
}
impl From<Vec2> for [f32; 2] {
    fn from(v: Vec2) -> Self {
        [v.x, v.y]
    }
}
impl From<[f32; 3]> for Vec3 {
    fn from([x, y, z]: [f32; 3]) -> Self {
        Self::new(x, y, z)
    }
}
impl From<(f32, f32, f32)> for Vec3 {
    fn from((x, y, z): (f32, f32, f32)) -> Self {
        Self::new(x, y, z)
    }
}
impl From<Vec3> for [f32; 3] {
    fn from(v: Vec3) -> Self {
        [v.x, v.y, v.z]
    }
}
/// `[x, y, z, w]`.
impl From<[f32; 4]> for Quat {
    fn from([x, y, z, w]: [f32; 4]) -> Self {
        Self::new(x, y, z, w)
    }
}
/// `(x, y, z, w)`.
impl From<(f32, f32, f32, f32)> for Quat {
    fn from((x, y, z, w): (f32, f32, f32, f32)) -> Self {
        Self::new(x, y, z, w)
    }
}
impl From<Quat> for [f32; 4] {
    fn from(q: Quat) -> Self {
        [q.x, q.y, q.z, q.w]
    }
}
/// Column-major.
impl From<[f32; 16]> for Mat4 {
    fn from(m: [f32; 16]) -> Self {
        Self { m }
    }
}
/// Four columns.
impl From<[[f32; 4]; 4]> for Mat4 {
    fn from(columns: [[f32; 4]; 4]) -> Self {
        Self { m: std::array::from_fn(|i| columns[i / 4][i % 4]) }
    }
}
impl From<Mat4> for [f32; 16] {
    fn from(m: Mat4) -> Self {
        m.m
    }
}

// ----- Vector math -----
macro_rules! vec_ops {
    ($t:ident { $($f:ident),* }) => {
        impl Add for $t {
            type Output = Self;
            fn add(self, o: Self) -> Self { Self { $($f: self.$f + o.$f),* } }
        }
        impl Sub for $t {
            type Output = Self;
            fn sub(self, o: Self) -> Self { Self { $($f: self.$f - o.$f),* } }
        }
        impl Mul<f32> for $t {
            type Output = Self;
            fn mul(self, s: f32) -> Self { Self { $($f: self.$f * s),* } }
        }
        impl Mul<$t> for f32 {
            type Output = $t;
            fn mul(self, v: $t) -> $t { v * self }
        }
        impl Neg for $t {
            type Output = Self;
            fn neg(self) -> Self { Self { $($f: -self.$f),* } }
        }
        impl AddAssign for $t {
            fn add_assign(&mut self, o: Self) { *self = *self + o; }
        }
        impl SubAssign for $t {
            fn sub_assign(&mut self, o: Self) { *self = *self - o; }
        }
    };
}
vec_ops!(Vec2 { x, y });
vec_ops!(Vec3 { x, y, z });

// ----- Conversions to and from the C types (field by field, no casts between layouts) -----
impl From<NreVec2> for Vec2 {
    fn from(v: NreVec2) -> Self {
        Self::new(v.x, v.y)
    }
}
impl From<Vec2> for NreVec2 {
    fn from(v: Vec2) -> Self {
        NreVec2 { x: v.x, y: v.y }
    }
}
impl From<Vec3> for NreVec3 {
    fn from(v: Vec3) -> Self {
        NreVec3 { x: v.x, y: v.y, z: v.z }
    }
}
impl From<NreVec3> for Vec3 {
    fn from(v: NreVec3) -> Self {
        Self::new(v.x, v.y, v.z)
    }
}
impl From<NreMat4> for Mat4 {
    fn from(m: NreMat4) -> Self {
        Self { m: m.m }
    }
}
impl From<&Transform> for NreTransform {
    fn from(t: &Transform) -> Self {
        let r = t.rotation;
        NreTransform {
            position: t.position.into(),
            rotation: NreQuat { x: r.x, y: r.y, z: r.z, w: r.w },
            scale: t.scale.into(),
        }
    }
}
impl From<NreTransform> for Transform {
    fn from(t: NreTransform) -> Self {
        let r = t.rotation;
        Self::new(t.position.into(), Quat::new(r.x, r.y, r.z, r.w), t.scale.into())
    }
}
impl From<NreRayHit> for RayHit {
    fn from(h: NreRayHit) -> Self {
        Self { instance: Instance::from_raw(h.instance), distance: h.distance, point: h.point.into() }
    }
}
