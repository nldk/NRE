package nre

/*
#include "nre.h"
*/
import "C"

import "unsafe"

// cLayout lists the sizes and field offsets of nre.h's structs as cgo sees them, for the layout test
// (test files can't use cgo themselves).
func cLayout() map[string]uintptr {
	var t C.NreTransform
	var v C.NreVertex
	var r C.NreRayHit
	return map[string]uintptr{
		"Vertex":             unsafe.Sizeof(v),
		"Vertex.position":    unsafe.Offsetof(v.position),
		"Vertex.normal":      unsafe.Offsetof(v.normal),
		"Vertex.uv":          unsafe.Offsetof(v.uv),
		"RayHit":             unsafe.Sizeof(r),
		"RayHit.instance":    unsafe.Offsetof(r.instance),
		"RayHit.distance":    unsafe.Offsetof(r.distance),
		"RayHit.point":       unsafe.Offsetof(r.point),
		"SceneLighting":      unsafe.Sizeof(C.NreSceneLighting(0)),
		"Vec2":               unsafe.Sizeof(C.NreVec2{}),
		"Vec3":               unsafe.Sizeof(C.NreVec3{}),
		"Quat":               unsafe.Sizeof(C.NreQuat{}),
		"Mat4":               unsafe.Sizeof(C.NreMat4{}),
		"Transform":          unsafe.Sizeof(t),
		"Transform.position": unsafe.Offsetof(t.position),
		"Transform.rotation": unsafe.Offsetof(t.rotation),
		"Transform.scale":    unsafe.Offsetof(t.scale),
		"Shader":             unsafe.Sizeof(C.NreShader{}),
		"Pipeline":           unsafe.Sizeof(C.NrePipeline{}),
		"Model":              unsafe.Sizeof(C.NreModel{}),
		"Instance":           unsafe.Sizeof(C.NreInstance{}),
		"Light":              unsafe.Sizeof(C.NreLight{}),
		"Scene":              unsafe.Sizeof(C.NreScene{}),
		"Texture":            unsafe.Sizeof(C.NreTexture{}),
		"Material":           unsafe.Sizeof(C.NreMaterial{}),
		"Cubemap":            unsafe.Sizeof(C.NreCubemap{}),
		"ErrorCode":          unsafe.Sizeof(C.NreErrorCode(0)),
		"Key":                unsafe.Sizeof(C.NreKey(0)),
	}
}

// roundTrip converts the Go values to the C structs and back, for the conversion test.
func roundTrip(t Transform, h Model) (Transform, Model) {
	return transformFromC(t.c()), modelFromC(h.c())
}

// rayHitRoundTrip converts a C NreRayHit built from these values to a RayHit, for the conversion test.
func rayHitRoundTrip(h RayHit) RayHit {
	return rayHitFromC(C.NreRayHit{instance: h.Instance.c(), distance: C.float(h.Distance), point: h.Point.c()})
}

// rawError builds an error from a raw C code, for the unknown-code test.
func rawError(code int64, message string) *Error { return newError(C.NreErrorCode(code), message) }
