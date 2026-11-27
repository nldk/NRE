package nre

/*
#include "nre.h"
*/
import "C"

import "unsafe"

// Vec2 is NreVec2.
type Vec2 struct{ X, Y float32 }

// Vec3 is NreVec3.
type Vec3 struct{ X, Y, Z float32 }

// Quat is NreQuat, a rotation quaternion. Note the order: X, Y, Z, then W. See QuatIdentity.
type Quat struct{ X, Y, Z, W float32 }

// Mat4 is NreMat4: 16 floats, column-major (like glm and GLSL): m[column*4+row].
type Mat4 [16]float32

// Transform is NreTransform: applied as scale, then rotation, then translation. The zero value is NOT
// the identity (its scale is 0); use IdentityTransform.
type Transform struct {
	Position Vec3
	Rotation Quat
	Scale    Vec3
}

// Vertex is NreVertex: one vertex for CreateMeshModel. A Normal of {0,0,0} is computed from the triangles
// that use the vertex. CreateMeshModel hands a []Vertex to C as it is, so the layout matches NreVertex
// (checked at compile time below).
type Vertex struct {
	Position Vec3
	Normal   Vec3
	UV       Vec2
}

// RayHit is NreRayHit, what PickInstance hit: the instance, the distance from the camera along the ray
// (world units) and the world-space point.
type RayHit struct {
	Instance Instance
	Distance float32
	Point    Vec3
}

// Compile-time layout check for passing []Vertex to C without copying: each index below is 0 only if the
// Go and C sizes/offsets are equal; anything else is an out-of-range constant index, a compile error.
func _() {
	var c C.NreVertex
	var g Vertex
	var x [1]struct{}
	_ = x[unsafe.Sizeof(g)^unsafe.Sizeof(c)]
	_ = x[unsafe.Offsetof(g.Position)^unsafe.Offsetof(c.position)]
	_ = x[unsafe.Offsetof(g.Normal)^unsafe.Offsetof(c.normal)]
	_ = x[unsafe.Offsetof(g.UV)^unsafe.Offsetof(c.uv)]
	_ = x[unsafe.Sizeof(g.UV.X)^unsafe.Sizeof(c.uv.x)]
}

// Add returns v + o.
func (v Vec2) Add(o Vec2) Vec2 { return Vec2{v.X + o.X, v.Y + o.Y} }

// Sub returns v - o.
func (v Vec2) Sub(o Vec2) Vec2 { return Vec2{v.X - o.X, v.Y - o.Y} }

// Scale returns v * s.
func (v Vec2) Scale(s float32) Vec2 { return Vec2{v.X * s, v.Y * s} }

// Add returns v + o.
func (v Vec3) Add(o Vec3) Vec3 { return Vec3{v.X + o.X, v.Y + o.Y, v.Z + o.Z} }

// Sub returns v - o.
func (v Vec3) Sub(o Vec3) Vec3 { return Vec3{v.X - o.X, v.Y - o.Y, v.Z - o.Z} }

// Scale returns v * s.
func (v Vec3) Scale(s float32) Vec3 { return Vec3{v.X * s, v.Y * s, v.Z * s} }

// QuatIdentity returns the identity rotation {0, 0, 0, 1}.
func QuatIdentity() Quat { return Quat{0, 0, 0, 1} }

// IdentityTransform returns position {0,0,0}, rotation QuatIdentity(), scale {1,1,1}.
func IdentityTransform() Transform {
	return Transform{Rotation: QuatIdentity(), Scale: Vec3{1, 1, 1}}
}

// At returns the element in the given column and row.
func (m Mat4) At(column, row int) float32 { return m[column*4+row] }

// Conversions to and from the C structs.

func (v Vec2) c() C.NreVec2 { return C.NreVec2{x: C.float(v.X), y: C.float(v.Y)} }

func (v Vec3) c() C.NreVec3 { return C.NreVec3{x: C.float(v.X), y: C.float(v.Y), z: C.float(v.Z)} }

func vec2FromC(v C.NreVec2) Vec2 { return Vec2{float32(v.x), float32(v.y)} }
func vec3FromC(v C.NreVec3) Vec3 { return Vec3{float32(v.x), float32(v.y), float32(v.z)} }

func (q Quat) c() C.NreQuat {
	return C.NreQuat{x: C.float(q.X), y: C.float(q.Y), z: C.float(q.Z), w: C.float(q.W)}
}

func quatFromC(q C.NreQuat) Quat { return Quat{float32(q.x), float32(q.y), float32(q.z), float32(q.w)} }

func (t Transform) c() C.NreTransform {
	return C.NreTransform{position: t.Position.c(), rotation: t.Rotation.c(), scale: t.Scale.c()}
}

func transformFromC(t C.NreTransform) Transform {
	return Transform{vec3FromC(t.position), quatFromC(t.rotation), vec3FromC(t.scale)}
}

func rayHitFromC(h C.NreRayHit) RayHit {
	return RayHit{instanceFromC(h.instance), float32(h.distance), vec3FromC(h.point)}
}

func mat4FromC(m C.NreMat4) Mat4 {
	var r Mat4
	for i := range r {
		r[i] = float32(m.m[i])
	}
	return r
}
