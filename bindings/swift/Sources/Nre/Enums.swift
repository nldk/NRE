import CNre

// The enums of nre.h, with the same values (nre.h never changes them; new ones are added at the end).
// Names follow renderer.h, in Swift style.

/// Error codes: NreErrorCode in nre.h. `NreError.code` never holds `.ok`.
public enum ErrorCode: Int32, Sendable, CaseIterable, CustomStringConvertible {
    case ok = 0
    case unsupportedGPU = 1
    case errorCreatingVulkanInstance = 2
    case errorCreatingSwapchain = 3
    case errorRecreatingSwapchain = 4
    case errorCreatingPipeline = 5
    case windowCreationFailed = 6
    case fileNotFound = 7
    case wrongMode = 8
    case notInitialized = 9
    case alreadyInitialized = 10
    /// The handle is empty, destroyed, or was never created.
    case invalidHandle = 11
    case unsupportedFormat = 12
    case modelLoadFailed = 13
    case failedToCreateAllocator = 14
    case bufferCreationFailed = 15
    case uploadFailed = 16
    case noPipelineBound = 17
    case noSceneBound = 18
    case errorCreatingTexture = 19
    case imageCreationFailed = 20
    case imageViewCreationFailed = 21
    case indexOutOfRange = 22
    case impossibleTypeForPerFrameBuffer = 23
    case bufferWithSizeNull = 24
    case sizeOfBufferTooBig = 25
    case descriptorCreationFailed = 26
    case shaderCreationFailed = 27
    /// Also: a C++ exception inside the renderer, or a code these bindings don't know.
    case unknown = 28
    case libraryLoadFailed = 29
    /// E.g. a negative shadow distance, or picking in render2D.
    case invalidArgument = 30

    /// Never fails: values these bindings don't know become `.unknown`.
    public init(code: Int32) { self = ErrorCode(rawValue: code) ?? .unknown }

    init(_ c: NreErrorCode) { self.init(code: Int32(truncatingIfNeeded: c.rawValue)) }

    /// "Ok", "FileNotFound", ...: nreErrorCodeToString.
    public var description: String { NRE.errorCodeToString(self) }
}

public enum ShaderType: Int32, Sendable, CaseIterable {
    case fragment = 0
    case vertex = 1
    case geometry = 2
    case compute = 3
}

public enum RenderMode: Int32, Sendable, CaseIterable {
    case render3D = 0
    case render2D = 1
}

/// See renderer.h's renderFrame for what each mode does.
public enum ApiMode: Int32, Sendable, CaseIterable {
    case sceneMode = 0
    case functionDrawMode = 1
}

/// A scene's lighting mode (setSceneLighting); see renderer.h's SceneLighting.
public enum SceneLighting: Int32, Sendable, CaseIterable {
    /// The default: the cubemap or the sky color, short sharp shadows, no haze.
    case small = 0
    /// Procedural sky from the sun, far cascaded shadows, haze, automatic exposure.
    case landscape = 1

    /// nil for a value these bindings don't know.
    init?(_ c: NreSceneLighting) { self.init(rawValue: Int32(truncatingIfNeeded: c.rawValue)) }
}

/// What the renderer does with errors inside a frame; see renderer.h.
public enum RenderErrorPolicy: Int32, Sendable, CaseIterable {
    case ignore = 0
    case `return` = 1
    case panic = 2
}

public enum MouseButton: Int32, Sendable, CaseIterable {
    case left = 0
    case right = 1
    case middle = 2
}

public enum CursorMode: Int32, Sendable, CaseIterable {
    case normal = 0
    /// Invisible over the window, but moves and leaves it as usual.
    case hidden = 1
    /// Hidden and kept in the window, unlimited movement: for mouse-look cameras.
    case locked = 2
}

/// Physical keys, named after the US layout. The values are GLFW's key codes.
public enum Key: Int32, Sendable, CaseIterable {
    case space = 32, apostrophe = 39, comma = 44, minus = 45, period = 46, slash = 47
    case zero = 48, one = 49, two = 50, three = 51, four = 52
    case five = 53, six = 54, seven = 55, eight = 56, nine = 57
    case semicolon = 59, equal = 61
    case a = 65, b = 66, c = 67, d = 68, e = 69, f = 70, g = 71, h = 72, i = 73, j = 74, k = 75, l = 76
    case m = 77, n = 78, o = 79, p = 80, q = 81, r = 82, s = 83, t = 84, u = 85, v = 86, w = 87, x = 88
    case y = 89, z = 90
    case leftBracket = 91, backslash = 92, rightBracket = 93, graveAccent = 96
    case escape = 256, enter = 257, tab = 258, backspace = 259, insert = 260, delete = 261
    case right = 262, left = 263, down = 264, up = 265, pageUp = 266, pageDown = 267, home = 268, end = 269
    case capsLock = 280, scrollLock = 281, numLock = 282, printScreen = 283, pause = 284
    case f1 = 290, f2 = 291, f3 = 292, f4 = 293, f5 = 294, f6 = 295
    case f7 = 296, f8 = 297, f9 = 298, f10 = 299, f11 = 300, f12 = 301
    case keypad0 = 320, keypad1 = 321, keypad2 = 322, keypad3 = 323, keypad4 = 324
    case keypad5 = 325, keypad6 = 326, keypad7 = 327, keypad8 = 328, keypad9 = 329
    case keypadDecimal = 330, keypadDivide = 331, keypadMultiply = 332
    case keypadSubtract = 333, keypadAdd = 334, keypadEnter = 335, keypadEqual = 336
    case leftShift = 340, leftControl = 341, leftAlt = 342, leftSuper = 343
    case rightShift = 344, rightControl = 345, rightAlt = 346, rightSuper = 347, menu = 348
}

// To the C enums. Imported C enums are RawRepresentable structs whose raw type depends on the platform
// (UInt32 on Linux/macOS, Int32 on Windows), so convert through it.
extension ErrorCode { var c: NreErrorCode { .init(rawValue: .init(truncatingIfNeeded: rawValue)) } }
extension ShaderType { var c: NreShaderType { .init(rawValue: .init(truncatingIfNeeded: rawValue)) } }
extension RenderMode { var c: NreRenderMode { .init(rawValue: .init(truncatingIfNeeded: rawValue)) } }
extension ApiMode { var c: NreApiMode { .init(rawValue: .init(truncatingIfNeeded: rawValue)) } }
extension SceneLighting { var c: NreSceneLighting { .init(rawValue: .init(truncatingIfNeeded: rawValue)) } }
extension RenderErrorPolicy { var c: NreRenderErrorPolicy { .init(rawValue: .init(truncatingIfNeeded: rawValue)) } }
extension MouseButton { var c: NreMouseButton { .init(rawValue: .init(truncatingIfNeeded: rawValue)) } }
extension CursorMode { var c: NreCursorMode { .init(rawValue: .init(truncatingIfNeeded: rawValue)) } }
extension Key { var c: NreKey { .init(rawValue: .init(truncatingIfNeeded: rawValue)) } }
