namespace Nre;

// The enums of nre.h, with the same values (nre.h never changes them; new ones are added at the end).
// Names follow renderer.h, in C# style.

/// <summary>Error codes, NreErrorCode in nre.h. <see cref="NreException.Code"/> never holds <see cref="Ok"/>.</summary>
public enum ErrorCode
{
    Ok = 0,
    UnsupportedGpu = 1,
    ErrorCreatingVulkanInstance = 2,
    ErrorCreatingSwapchain = 3,
    ErrorRecreatingSwapchain = 4,
    ErrorCreatingPipeline = 5,
    WindowCreationFailed = 6,
    FileNotFound = 7,
    WrongMode = 8,
    NotInitialized = 9,
    AlreadyInitialized = 10,
    /// <summary>The handle is empty, destroyed, or was never created.</summary>
    InvalidHandle = 11,
    UnsupportedFormat = 12,
    ModelLoadFailed = 13,
    FailedToCreateAllocator = 14,
    BufferCreationFailed = 15,
    UploadFailed = 16,
    NoPipelineBound = 17,
    NoSceneBound = 18,
    ErrorCreatingTexture = 19,
    ImageCreationFailed = 20,
    ImageViewCreationFailed = 21,
    IndexOutOfRange = 22,
    ImpossibleTypeForPerFrameBuffer = 23,
    BufferWithSizeNull = 24,
    SizeOfBufferTooBig = 25,
    DescriptorCreationFailed = 26,
    ShaderCreationFailed = 27,
    /// <summary>Also: a C++ exception inside the renderer, or a code these bindings don't know.</summary>
    Unknown = 28,
    LibraryLoadFailed = 29,
    /// <summary>E.g. a negative shadow distance, or picking in Render2D.</summary>
    InvalidArgument = 30,
}

public enum ShaderType
{
    Fragment = 0,
    Vertex = 1,
    Geometry = 2,
    Compute = 3,
}

public enum RenderMode
{
    Render3D = 0,
    Render2D = 1,
}

/// <summary>See renderer.h's renderFrame for what each mode does.</summary>
public enum ApiMode
{
    SceneMode = 0,
    FunctionDrawMode = 1,
}

/// <summary>A scene's lighting mode (<see cref="NRE.SetSceneLighting"/>); see renderer.h's SceneLighting.</summary>
public enum SceneLighting
{
    /// <summary>Default: cubemap or sky color, short sharp shadows, no haze.</summary>
    Small = 0,
    /// <summary>Procedural sky from the sun, far cascaded shadows, haze, automatic exposure.</summary>
    Landscape = 1,
}

public enum RenderErrorPolicy
{
    Ignore = 0,
    Return = 1,
    Panic = 2,
}

public enum MouseButton
{
    Left = 0,
    Right = 1,
    Middle = 2,
}

public enum CursorMode
{
    Normal = 0,
    /// <summary>Invisible over the window, but moves and leaves it as usual.</summary>
    Hidden = 1,
    /// <summary>Hidden and kept in the window, unlimited movement: for mouse-look cameras.</summary>
    Locked = 2,
}

/// <summary>Physical keys, named after the US layout. The values are GLFW's key codes.</summary>
public enum Key
{
    Space = 32, Apostrophe = 39, Comma = 44, Minus = 45, Period = 46, Slash = 47,
    Zero = 48, One = 49, Two = 50, Three = 51, Four = 52,
    Five = 53, Six = 54, Seven = 55, Eight = 56, Nine = 57,
    Semicolon = 59, Equal = 61,
    A = 65, B = 66, C = 67, D = 68, E = 69, F = 70, G = 71, H = 72, I = 73, J = 74, K = 75, L = 76,
    M = 77, N = 78, O = 79, P = 80, Q = 81, R = 82, S = 83, T = 84, U = 85, V = 86, W = 87, X = 88,
    Y = 89, Z = 90,
    LeftBracket = 91, Backslash = 92, RightBracket = 93, GraveAccent = 96,
    Escape = 256, Enter = 257, Tab = 258, Backspace = 259, Insert = 260, Delete = 261,
    Right = 262, Left = 263, Down = 264, Up = 265, PageUp = 266, PageDown = 267, Home = 268, End = 269,
    CapsLock = 280, ScrollLock = 281, NumLock = 282, PrintScreen = 283, Pause = 284,
    F1 = 290, F2 = 291, F3 = 292, F4 = 293, F5 = 294, F6 = 295,
    F7 = 296, F8 = 297, F9 = 298, F10 = 299, F11 = 300, F12 = 301,
    Keypad0 = 320, Keypad1 = 321, Keypad2 = 322, Keypad3 = 323, Keypad4 = 324,
    Keypad5 = 325, Keypad6 = 326, Keypad7 = 327, Keypad8 = 328, Keypad9 = 329,
    KeypadDecimal = 330, KeypadDivide = 331, KeypadMultiply = 332, KeypadSubtract = 333,
    KeypadAdd = 334, KeypadEnter = 335, KeypadEqual = 336,
    LeftShift = 340, LeftControl = 341, LeftAlt = 342, LeftSuper = 343,
    RightShift = 344, RightControl = 345, RightAlt = 346, RightSuper = 347, Menu = 348,
}
