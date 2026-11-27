package nre

/*
#include "nre.h"
*/
import "C"

import "fmt"

// ShaderType is NreShaderType: the stage a SPIR-V file is for.
type ShaderType int32

const (
	ShaderFragment ShaderType = C.NRE_SHADER_FRAGMENT
	ShaderVertex   ShaderType = C.NRE_SHADER_VERTEX
	ShaderGeometry ShaderType = C.NRE_SHADER_GEOMETRY
	ShaderCompute  ShaderType = C.NRE_SHADER_COMPUTE
)

// RenderMode is NreRenderMode.
type RenderMode int32

const (
	Render3D RenderMode = C.NRE_RENDER_3D
	Render2D RenderMode = C.NRE_RENDER_2D
)

// ApiMode is NreApiMode. SceneMode: RenderFrame draws the bound scene. FunctionDrawMode: RenderFrame starts
// a frame, then DrawModel/DrawInstance calls, then EndFrame (raylib style). See renderer.h.
type ApiMode int32

const (
	SceneMode        ApiMode = C.NRE_API_MODE_SCENE
	FunctionDrawMode ApiMode = C.NRE_API_MODE_FUNCTION_DRAW
)

// SceneLighting is NreSceneLighting: a scene's lighting mode (SetSceneLighting). See renderer.h.
// GetSceneLighting returns the library's value as is, so a mode from a newer library is just another number
// (String shows it as SceneLighting(n)).
type SceneLighting int32

const (
	SceneLightingSmall     SceneLighting = C.NRE_SCENE_LIGHTING_SMALL     // default: cubemap or sky colour, short sharp shadows, no haze
	SceneLightingLandscape SceneLighting = C.NRE_SCENE_LIGHTING_LANDSCAPE // procedural sky from the sun, far cascaded shadows, haze, auto exposure
)

// String returns "Small", "Landscape", or "SceneLighting(n)" for a value these bindings don't know.
func (l SceneLighting) String() string {
	switch l {
	case SceneLightingSmall:
		return "Small"
	case SceneLightingLandscape:
		return "Landscape"
	}
	return fmt.Sprintf("SceneLighting(%d)", int32(l))
}

// RenderErrorPolicy is NreRenderErrorPolicy: what the renderer does with errors inside a frame.
type RenderErrorPolicy int32

const (
	ErrorPolicyIgnore RenderErrorPolicy = C.NRE_ERROR_POLICY_IGNORE
	ErrorPolicyReturn RenderErrorPolicy = C.NRE_ERROR_POLICY_RETURN
	ErrorPolicyPanic  RenderErrorPolicy = C.NRE_ERROR_POLICY_PANIC
)

// MouseButton is NreMouseButton.
type MouseButton int32

const (
	MouseButtonLeft   MouseButton = C.NRE_MOUSE_BUTTON_LEFT
	MouseButtonRight  MouseButton = C.NRE_MOUSE_BUTTON_RIGHT
	MouseButtonMiddle MouseButton = C.NRE_MOUSE_BUTTON_MIDDLE
)

// CursorMode is NreCursorMode.
type CursorMode int32

const (
	CursorNormal CursorMode = C.NRE_CURSOR_NORMAL
	CursorHidden CursorMode = C.NRE_CURSOR_HIDDEN // invisible over the window, but moves and leaves it as usual
	CursorLocked CursorMode = C.NRE_CURSOR_LOCKED // hidden and kept in the window, unlimited movement: for mouse-look cameras
)

// Key is NreKey: a physical key, named after the US layout. The values are GLFW's key codes.
type Key int32

const (
	KeySpace        Key = C.NRE_KEY_SPACE
	KeyApostrophe   Key = C.NRE_KEY_APOSTROPHE
	KeyComma        Key = C.NRE_KEY_COMMA
	KeyMinus        Key = C.NRE_KEY_MINUS
	KeyPeriod       Key = C.NRE_KEY_PERIOD
	KeySlash        Key = C.NRE_KEY_SLASH
	Key0            Key = C.NRE_KEY_0
	Key1            Key = C.NRE_KEY_1
	Key2            Key = C.NRE_KEY_2
	Key3            Key = C.NRE_KEY_3
	Key4            Key = C.NRE_KEY_4
	Key5            Key = C.NRE_KEY_5
	Key6            Key = C.NRE_KEY_6
	Key7            Key = C.NRE_KEY_7
	Key8            Key = C.NRE_KEY_8
	Key9            Key = C.NRE_KEY_9
	KeySemicolon    Key = C.NRE_KEY_SEMICOLON
	KeyEqual        Key = C.NRE_KEY_EQUAL
	KeyA            Key = C.NRE_KEY_A
	KeyB            Key = C.NRE_KEY_B
	KeyC            Key = C.NRE_KEY_C
	KeyD            Key = C.NRE_KEY_D
	KeyE            Key = C.NRE_KEY_E
	KeyF            Key = C.NRE_KEY_F
	KeyG            Key = C.NRE_KEY_G
	KeyH            Key = C.NRE_KEY_H
	KeyI            Key = C.NRE_KEY_I
	KeyJ            Key = C.NRE_KEY_J
	KeyK            Key = C.NRE_KEY_K
	KeyL            Key = C.NRE_KEY_L
	KeyM            Key = C.NRE_KEY_M
	KeyN            Key = C.NRE_KEY_N
	KeyO            Key = C.NRE_KEY_O
	KeyP            Key = C.NRE_KEY_P
	KeyQ            Key = C.NRE_KEY_Q
	KeyR            Key = C.NRE_KEY_R
	KeyS            Key = C.NRE_KEY_S
	KeyT            Key = C.NRE_KEY_T
	KeyU            Key = C.NRE_KEY_U
	KeyV            Key = C.NRE_KEY_V
	KeyW            Key = C.NRE_KEY_W
	KeyX            Key = C.NRE_KEY_X
	KeyY            Key = C.NRE_KEY_Y
	KeyZ            Key = C.NRE_KEY_Z
	KeyLeftBracket  Key = C.NRE_KEY_LEFT_BRACKET
	KeyBackslash    Key = C.NRE_KEY_BACKSLASH
	KeyRightBracket Key = C.NRE_KEY_RIGHT_BRACKET
	KeyGraveAccent  Key = C.NRE_KEY_GRAVE_ACCENT
	KeyEscape       Key = C.NRE_KEY_ESCAPE
	KeyEnter        Key = C.NRE_KEY_ENTER
	KeyTab          Key = C.NRE_KEY_TAB
	KeyBackspace    Key = C.NRE_KEY_BACKSPACE
	KeyInsert       Key = C.NRE_KEY_INSERT
	KeyDelete       Key = C.NRE_KEY_DELETE
	KeyRight        Key = C.NRE_KEY_RIGHT
	KeyLeft         Key = C.NRE_KEY_LEFT
	KeyDown         Key = C.NRE_KEY_DOWN
	KeyUp           Key = C.NRE_KEY_UP
	KeyPageUp       Key = C.NRE_KEY_PAGE_UP
	KeyPageDown     Key = C.NRE_KEY_PAGE_DOWN
	KeyHome         Key = C.NRE_KEY_HOME
	KeyEnd          Key = C.NRE_KEY_END
	KeyCapsLock     Key = C.NRE_KEY_CAPS_LOCK
	KeyScrollLock   Key = C.NRE_KEY_SCROLL_LOCK
	KeyNumLock      Key = C.NRE_KEY_NUM_LOCK
	KeyPrintScreen  Key = C.NRE_KEY_PRINT_SCREEN
	KeyPause        Key = C.NRE_KEY_PAUSE
	KeyF1           Key = C.NRE_KEY_F1
	KeyF2           Key = C.NRE_KEY_F2
	KeyF3           Key = C.NRE_KEY_F3
	KeyF4           Key = C.NRE_KEY_F4
	KeyF5           Key = C.NRE_KEY_F5
	KeyF6           Key = C.NRE_KEY_F6
	KeyF7           Key = C.NRE_KEY_F7
	KeyF8           Key = C.NRE_KEY_F8
	KeyF9           Key = C.NRE_KEY_F9
	KeyF10          Key = C.NRE_KEY_F10
	KeyF11          Key = C.NRE_KEY_F11
	KeyF12          Key = C.NRE_KEY_F12
	KeyKP0          Key = C.NRE_KEY_KP_0
	KeyKP1          Key = C.NRE_KEY_KP_1
	KeyKP2          Key = C.NRE_KEY_KP_2
	KeyKP3          Key = C.NRE_KEY_KP_3
	KeyKP4          Key = C.NRE_KEY_KP_4
	KeyKP5          Key = C.NRE_KEY_KP_5
	KeyKP6          Key = C.NRE_KEY_KP_6
	KeyKP7          Key = C.NRE_KEY_KP_7
	KeyKP8          Key = C.NRE_KEY_KP_8
	KeyKP9          Key = C.NRE_KEY_KP_9
	KeyKPDecimal    Key = C.NRE_KEY_KP_DECIMAL
	KeyKPDivide     Key = C.NRE_KEY_KP_DIVIDE
	KeyKPMultiply   Key = C.NRE_KEY_KP_MULTIPLY
	KeyKPSubtract   Key = C.NRE_KEY_KP_SUBTRACT
	KeyKPAdd        Key = C.NRE_KEY_KP_ADD
	KeyKPEnter      Key = C.NRE_KEY_KP_ENTER
	KeyKPEqual      Key = C.NRE_KEY_KP_EQUAL
	KeyLeftShift    Key = C.NRE_KEY_LEFT_SHIFT
	KeyLeftControl  Key = C.NRE_KEY_LEFT_CONTROL
	KeyLeftAlt      Key = C.NRE_KEY_LEFT_ALT
	KeyLeftSuper    Key = C.NRE_KEY_LEFT_SUPER
	KeyRightShift   Key = C.NRE_KEY_RIGHT_SHIFT
	KeyRightControl Key = C.NRE_KEY_RIGHT_CONTROL
	KeyRightAlt     Key = C.NRE_KEY_RIGHT_ALT
	KeyRightSuper   Key = C.NRE_KEY_RIGHT_SUPER
	KeyMenu         Key = C.NRE_KEY_MENU
)
