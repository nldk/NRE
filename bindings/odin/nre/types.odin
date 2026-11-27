package nre

// Types matching include/nre.h. Every enum is `enum c.int` (the C enums are kept 32-bit), every struct is a
// plain Odin struct (C layout), and the math types are Odin builtins whose memory layout is the C one
// (checked by the #asserts below and by the tests).

import "core:c"

// ---------------------------------------------------------------------------
// Enums. The values never change; new ones are added at the end.
// ---------------------------------------------------------------------------

// NreErrorCode. `.Ok` is 0, so `or_return` works on the second result of every wrapper.
Error_Code :: enum c.int {
	Ok                                   = 0,
	Unsupported_Gpu                      = 1,
	Creating_Vulkan_Instance             = 2,
	Creating_Swapchain                   = 3,
	Recreating_Swapchain                 = 4,
	Creating_Pipeline                    = 5,
	Window_Creation_Failed               = 6,
	File_Not_Found                       = 7,
	Wrong_Mode                           = 8,
	Not_Initialized                      = 9,
	Already_Initialized                  = 10,
	Invalid_Handle                       = 11, // empty, destroyed, or never created
	Unsupported_Format                   = 12,
	Model_Load_Failed                    = 13,
	Failed_To_Create_Allocator           = 14,
	Buffer_Creation_Failed               = 15,
	Upload_Failed                        = 16,
	No_Pipeline_Bound                    = 17,
	No_Scene_Bound                       = 18,
	Creating_Texture                     = 19,
	Image_Creation_Failed                = 20,
	Image_View_Creation_Failed           = 21,
	Index_Out_Of_Range                   = 22,
	Impossible_Type_For_Per_Frame_Buffer = 23,
	Buffer_With_Size_Null                = 24,
	Size_Of_Buffer_Too_Big               = 25,
	Descriptor_Creation_Failed           = 26,
	Shader_Creation_Failed               = 27,
	Unknown                              = 28, // also: a C++ exception inside the renderer
	Library_Load_Failed                  = 29,
	Invalid_Argument                     = 30, // e.g. a negative shadow distance, or picking in Render_2D
}
// normalize_error_code relies on the values being 0..max with no gaps.
#assert(len(Error_Code) == int(max(Error_Code)) + 1)

Shader_Type :: enum c.int {
	Fragment = 0,
	Vertex   = 1,
	Geometry = 2,
	Compute  = 3,
}

Render_Mode :: enum c.int {
	Render_3D = 0,
	Render_2D = 1,
}

Api_Mode :: enum c.int {
	Scene         = 0, // render_frame() draws the bound scene
	Function_Draw = 1, // render_frame(), draw_model()/draw_instance() calls, end_frame()
}

// A scene's lighting mode (set_scene_lighting); see renderer.h's SceneLighting.
Scene_Lighting :: enum c.int {
	Small     = 0, // default: cubemap or sky colour, short sharp shadows, no haze
	Landscape = 1, // procedural sky from the sun, far cascaded shadows, haze, auto exposure
}

Render_Error_Policy :: enum c.int {
	Ignore = 0,
	Return = 1,
	Panic  = 2,
}

Mouse_Button :: enum c.int {
	Left   = 0,
	Right  = 1,
	Middle = 2,
}

Cursor_Mode :: enum c.int {
	Normal = 0,
	Hidden = 1, // invisible over the window, but moves and leaves it as usual
	Locked = 2, // hidden and kept in the window, unlimited movement: for mouse-look cameras
}

// Physical keys, named after the US layout. The values are GLFW's key codes.
Key :: enum c.int {
	Space = 32, Apostrophe = 39, Comma = 44, Minus = 45, Period = 46, Slash = 47,
	Zero = 48, One = 49, Two = 50, Three = 51, Four = 52,
	Five = 53, Six = 54, Seven = 55, Eight = 56, Nine = 57,
	Semicolon = 59, Equal = 61,
	A = 65, B = 66, C = 67, D = 68, E = 69, F = 70, G = 71, H = 72, I = 73, J = 74, K = 75, L = 76,
	M = 77, N = 78, O = 79, P = 80, Q = 81, R = 82, S = 83, T = 84, U = 85, V = 86, W = 87, X = 88,
	Y = 89, Z = 90,
	Left_Bracket = 91, Backslash = 92, Right_Bracket = 93, Grave_Accent = 96,
	Escape = 256, Enter = 257, Tab = 258, Backspace = 259, Insert = 260, Delete = 261,
	Right = 262, Left = 263, Down = 264, Up = 265, Page_Up = 266, Page_Down = 267, Home = 268, End = 269,
	Caps_Lock = 280, Scroll_Lock = 281, Num_Lock = 282, Print_Screen = 283, Pause = 284,
	F1 = 290, F2 = 291, F3 = 292, F4 = 293, F5 = 294, F6 = 295,
	F7 = 296, F8 = 297, F9 = 298, F10 = 299, F11 = 300, F12 = 301,
	Keypad_0 = 320, Keypad_1 = 321, Keypad_2 = 322, Keypad_3 = 323, Keypad_4 = 324,
	Keypad_5 = 325, Keypad_6 = 326, Keypad_7 = 327, Keypad_8 = 328, Keypad_9 = 329,
	Keypad_Decimal = 330, Keypad_Divide = 331, Keypad_Multiply = 332, Keypad_Subtract = 333,
	Keypad_Add = 334, Keypad_Enter = 335, Keypad_Equal = 336,
	Left_Shift = 340, Left_Control = 341, Left_Alt = 342, Left_Super = 343,
	Right_Shift = 344, Right_Control = 345, Right_Alt = 346, Right_Super = 347, Menu = 348,
}

// ---------------------------------------------------------------------------
// Math types. Builtins, so core:math/linalg works on them directly. Angles are in radians.
// ---------------------------------------------------------------------------
Vec2 :: [2]f32                // NreVec2 {x, y}
Vec3 :: [3]f32                // NreVec3 {x, y, z}
Quat :: quaternion128         // NreQuat: stored x, y, z, w. Identity: Quat(1)
Mat4 :: matrix[4, 4]f32       // NreMat4: column-major, like m[column * 4 + row]. m[row, col] in Odin.

// One vertex for create_mesh_model. A normal of {0, 0, 0} is computed from the triangles using the vertex.
Vertex :: struct {
	position: Vec3,
	normal:   Vec3,
	uv:       Vec2,
}

// Applied as scale, then rotation, then translation. The zero value is NOT the identity (scale 0):
// use TRANSFORM_IDENTITY or transform(...).
Transform :: struct {
	position: Vec3,
	rotation: Quat,
	scale:    Vec3,
}

TRANSFORM_IDENTITY :: Transform{position = {0, 0, 0}, rotation = 1, scale = {1, 1, 1}}

// A transform from named parts, defaulting to the identity: transform(position = {0, 0, -1}).
transform :: proc "contextless" (position := Vec3{0, 0, 0}, rotation := Quat(1), scale := Vec3{1, 1, 1}) -> Transform {
	return {position, rotation, scale}
}

// ---------------------------------------------------------------------------
// Handles: values to copy around (index + generation); don't read or change the fields.
// The zero value {} is the empty handle, e.g. "no texture" in create_material or "no skybox" in set_skybox.
// Destroy them explicitly with the matching destroy_xxx, like the C++ API.
// ---------------------------------------------------------------------------
Shader   :: struct { index, generation: u32 }
Pipeline :: struct { index, generation: u32 }
Model    :: struct { index, generation: u32 }
Instance :: struct { index, generation: u32 }
Light    :: struct { index, generation: u32 }
Scene    :: struct { index, generation: u32 }
Texture  :: struct { index, generation: u32 }
Material :: struct { index, generation: u32 }
Cubemap  :: struct { index, generation: u32 }

// What pick_instance hit: the instance, the distance from the camera along the ray (world units) and the
// world-space point.
Ray_Hit :: struct {
	instance: Instance,
	distance: f32,
	point:    Vec3,
}

// Layouts of nre.h on every platform NRE supports.
#assert(size_of(Vec2) == 8 && align_of(Vec2) == 4)
#assert(size_of(Vec3) == 12 && align_of(Vec3) == 4)
#assert(size_of(Quat) == 16 && align_of(Quat) == 4)
#assert(size_of(Mat4) == 64 && align_of(Mat4) == 4)
#assert(size_of(Transform) == 40 && offset_of(Transform, rotation) == 12 && offset_of(Transform, scale) == 28)
#assert(size_of(Vertex) == 32 && align_of(Vertex) == 4 && offset_of(Vertex, normal) == 12 && offset_of(Vertex, uv) == 24)
#assert(size_of(Model) == 8 && align_of(Model) == 4)
#assert(size_of(Ray_Hit) == 24 && align_of(Ray_Hit) == 4 && offset_of(Ray_Hit, distance) == 8 && offset_of(Ray_Hit, point) == 12)
#assert(size_of(Error_Code) == 4 && size_of(Key) == 4 && size_of(Scene_Lighting) == 4)
#assert(size_of(bool) == 1) // C bool
