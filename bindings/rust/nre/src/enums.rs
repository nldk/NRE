//! The C enums as Rust enums. Converted to and from the C values with explicit matches, so a value the
//! library returns that these bindings don't know never becomes an invalid enum.

use nre_sys::*;

macro_rules! c_enum {
    (
        $(#[$meta:meta])*
        pub enum $name:ident : $raw:ty {
            $($(#[$vmeta:meta])* $variant:ident = $c:ident,)*
        }
    ) => {
        $(#[$meta])*
        #[non_exhaustive]
        #[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
        pub enum $name {
            $($(#[$vmeta])* $variant,)*
        }

        impl $name {
            /// The C value (see nre.h).
            pub const fn to_raw(self) -> $raw {
                match self {
                    $(Self::$variant => $c,)*
                }
            }

            /// The variant for a C value, or `None` for a value these bindings don't know.
            pub const fn from_raw(raw: $raw) -> Option<Self> {
                match raw {
                    $($c => Some(Self::$variant),)*
                    _ => None,
                }
            }
        }
    };
}

c_enum! {
    /// Why a call failed. See renderer.h's `ErrorCode`. A code from the library that these bindings don't know
    /// becomes [`ErrorCode::Unknown`].
    pub enum ErrorCode: NreErrorCode {
        UnsupportedGpu = NRE_ERROR_UNSUPPORTED_GPU,
        CreatingVulkanInstance = NRE_ERROR_CREATING_VULKAN_INSTANCE,
        CreatingSwapchain = NRE_ERROR_CREATING_SWAPCHAIN,
        RecreatingSwapchain = NRE_ERROR_RECREATING_SWAPCHAIN,
        CreatingPipeline = NRE_ERROR_CREATING_PIPELINE,
        WindowCreationFailed = NRE_ERROR_WINDOW_CREATION_FAILED,
        FileNotFound = NRE_ERROR_FILE_NOT_FOUND,
        WrongMode = NRE_ERROR_WRONG_MODE,
        NotInitialized = NRE_ERROR_NOT_INITIALIZED,
        AlreadyInitialized = NRE_ERROR_ALREADY_INITIALIZED,
        /// The handle is empty, destroyed, or was never created.
        InvalidHandle = NRE_ERROR_INVALID_HANDLE,
        UnsupportedFormat = NRE_ERROR_UNSUPPORTED_FORMAT,
        ModelLoadFailed = NRE_ERROR_MODEL_LOAD_FAILED,
        FailedToCreateAllocator = NRE_ERROR_FAILED_TO_CREATE_ALLOCATOR,
        BufferCreationFailed = NRE_ERROR_BUFFER_CREATION_FAILED,
        UploadFailed = NRE_ERROR_UPLOAD_FAILED,
        NoPipelineBound = NRE_ERROR_NO_PIPELINE_BOUND,
        NoSceneBound = NRE_ERROR_NO_SCENE_BOUND,
        CreatingTexture = NRE_ERROR_CREATING_TEXTURE,
        ImageCreationFailed = NRE_ERROR_IMAGE_CREATION_FAILED,
        ImageViewCreationFailed = NRE_ERROR_IMAGE_VIEW_CREATION_FAILED,
        IndexOutOfRange = NRE_ERROR_INDEX_OUT_OF_RANGE,
        ImpossibleTypeForPerFrameBuffer = NRE_ERROR_IMPOSSIBLE_TYPE_FOR_PER_FRAME_BUFFER,
        BufferWithSizeNull = NRE_ERROR_BUFFER_WITH_SIZE_NULL,
        SizeOfBufferTooBig = NRE_ERROR_SIZE_OF_BUFFER_TOO_BIG,
        DescriptorCreationFailed = NRE_ERROR_DESCRIPTOR_CREATION_FAILED,
        ShaderCreationFailed = NRE_ERROR_SHADER_CREATION_FAILED,
        /// A C++ exception inside the renderer, a code these bindings don't know, or a string argument the
        /// bindings couldn't pass to C (it contains a NUL byte, or is a non-UTF-8 path on Windows).
        Unknown = NRE_ERROR_UNKNOWN,
        /// Also: libnre_c isn't the version these bindings were written for (see [`crate::check_version`]).
        LibraryLoadFailed = NRE_ERROR_LIBRARY_LOAD_FAILED,
        /// E.g. a negative shadow distance, or picking in Render_2D.
        InvalidArgument = NRE_ERROR_INVALID_ARGUMENT,
    }
}

c_enum! {
    /// See renderer.h's `ShaderType`.
    pub enum ShaderType: NreShaderType {
        Fragment = NRE_SHADER_FRAGMENT,
        Vertex = NRE_SHADER_VERTEX,
        Geometry = NRE_SHADER_GEOMETRY,
        Compute = NRE_SHADER_COMPUTE,
    }
}

c_enum! {
    /// See renderer.h's `RenderMode`. Default: `Render3D`.
    #[derive(Default)]
    pub enum RenderMode: NreRenderMode {
        #[default]
        Render3D = NRE_RENDER_3D,
        Render2D = NRE_RENDER_2D,
    }
}

c_enum! {
    /// See renderer.h's `ApiMode` (`SceneMode` / `FunctionDrawMode`) and [`crate::render_frame`]. Default: `Scene`.
    #[derive(Default)]
    pub enum ApiMode: NreApiMode {
        #[default]
        Scene = NRE_API_MODE_SCENE,
        FunctionDraw = NRE_API_MODE_FUNCTION_DRAW,
    }
}

c_enum! {
    /// A scene's lighting mode ([`crate::set_scene_lighting`]); see renderer.h's `SceneLighting`. Default: `Small`.
    #[derive(Default)]
    pub enum SceneLighting: NreSceneLighting {
        /// The scene's cubemap, else the sky colour; one sharp shadow map over a short distance, no haze, exposure
        /// is exactly [`crate::set_exposure`].
        #[default]
        Small = NRE_SCENE_LIGHTING_SMALL,
        /// Procedural sky computed from the sun, cascaded shadows reaching far, haze, and an automatic exposure
        /// that follows the sun's height.
        Landscape = NRE_SCENE_LIGHTING_LANDSCAPE,
    }
}

c_enum! {
    /// See renderer.h's `RenderErrorPolicy`. Default: `Return`.
    #[derive(Default)]
    pub enum RenderErrorPolicy: NreRenderErrorPolicy {
        Ignore = NRE_ERROR_POLICY_IGNORE,
        #[default]
        Return = NRE_ERROR_POLICY_RETURN,
        Panic = NRE_ERROR_POLICY_PANIC,
    }
}

c_enum! {
    pub enum MouseButton: NreMouseButton {
        Left = NRE_MOUSE_BUTTON_LEFT,
        Right = NRE_MOUSE_BUTTON_RIGHT,
        Middle = NRE_MOUSE_BUTTON_MIDDLE,
    }
}

c_enum! {
    #[derive(Default)]
    pub enum CursorMode: NreCursorMode {
        #[default]
        Normal = NRE_CURSOR_NORMAL,
        /// Invisible over the window, but moves and leaves it as usual.
        Hidden = NRE_CURSOR_HIDDEN,
        /// Hidden and kept in the window, with unlimited movement: for mouse-look cameras.
        Locked = NRE_CURSOR_LOCKED,
    }
}

c_enum! {
    /// Physical keys, named after the US layout (`Key::W` is where W is on a US keyboard, whatever the layout).
    /// The C values are GLFW's key codes.
    pub enum Key: NreKey {
        Space = NRE_KEY_SPACE,
        Apostrophe = NRE_KEY_APOSTROPHE,
        Comma = NRE_KEY_COMMA,
        Minus = NRE_KEY_MINUS,
        Period = NRE_KEY_PERIOD,
        Slash = NRE_KEY_SLASH,
        Zero = NRE_KEY_0,
        One = NRE_KEY_1,
        Two = NRE_KEY_2,
        Three = NRE_KEY_3,
        Four = NRE_KEY_4,
        Five = NRE_KEY_5,
        Six = NRE_KEY_6,
        Seven = NRE_KEY_7,
        Eight = NRE_KEY_8,
        Nine = NRE_KEY_9,
        Semicolon = NRE_KEY_SEMICOLON,
        Equal = NRE_KEY_EQUAL,
        A = NRE_KEY_A,
        B = NRE_KEY_B,
        C = NRE_KEY_C,
        D = NRE_KEY_D,
        E = NRE_KEY_E,
        F = NRE_KEY_F,
        G = NRE_KEY_G,
        H = NRE_KEY_H,
        I = NRE_KEY_I,
        J = NRE_KEY_J,
        K = NRE_KEY_K,
        L = NRE_KEY_L,
        M = NRE_KEY_M,
        N = NRE_KEY_N,
        O = NRE_KEY_O,
        P = NRE_KEY_P,
        Q = NRE_KEY_Q,
        R = NRE_KEY_R,
        S = NRE_KEY_S,
        T = NRE_KEY_T,
        U = NRE_KEY_U,
        V = NRE_KEY_V,
        W = NRE_KEY_W,
        X = NRE_KEY_X,
        Y = NRE_KEY_Y,
        Z = NRE_KEY_Z,
        LeftBracket = NRE_KEY_LEFT_BRACKET,
        Backslash = NRE_KEY_BACKSLASH,
        RightBracket = NRE_KEY_RIGHT_BRACKET,
        GraveAccent = NRE_KEY_GRAVE_ACCENT,
        Escape = NRE_KEY_ESCAPE,
        Enter = NRE_KEY_ENTER,
        Tab = NRE_KEY_TAB,
        Backspace = NRE_KEY_BACKSPACE,
        Insert = NRE_KEY_INSERT,
        Delete = NRE_KEY_DELETE,
        Right = NRE_KEY_RIGHT,
        Left = NRE_KEY_LEFT,
        Down = NRE_KEY_DOWN,
        Up = NRE_KEY_UP,
        PageUp = NRE_KEY_PAGE_UP,
        PageDown = NRE_KEY_PAGE_DOWN,
        Home = NRE_KEY_HOME,
        End = NRE_KEY_END,
        CapsLock = NRE_KEY_CAPS_LOCK,
        ScrollLock = NRE_KEY_SCROLL_LOCK,
        NumLock = NRE_KEY_NUM_LOCK,
        PrintScreen = NRE_KEY_PRINT_SCREEN,
        Pause = NRE_KEY_PAUSE,
        F1 = NRE_KEY_F1,
        F2 = NRE_KEY_F2,
        F3 = NRE_KEY_F3,
        F4 = NRE_KEY_F4,
        F5 = NRE_KEY_F5,
        F6 = NRE_KEY_F6,
        F7 = NRE_KEY_F7,
        F8 = NRE_KEY_F8,
        F9 = NRE_KEY_F9,
        F10 = NRE_KEY_F10,
        F11 = NRE_KEY_F11,
        F12 = NRE_KEY_F12,
        Keypad0 = NRE_KEY_KP_0,
        Keypad1 = NRE_KEY_KP_1,
        Keypad2 = NRE_KEY_KP_2,
        Keypad3 = NRE_KEY_KP_3,
        Keypad4 = NRE_KEY_KP_4,
        Keypad5 = NRE_KEY_KP_5,
        Keypad6 = NRE_KEY_KP_6,
        Keypad7 = NRE_KEY_KP_7,
        Keypad8 = NRE_KEY_KP_8,
        Keypad9 = NRE_KEY_KP_9,
        KeypadDecimal = NRE_KEY_KP_DECIMAL,
        KeypadDivide = NRE_KEY_KP_DIVIDE,
        KeypadMultiply = NRE_KEY_KP_MULTIPLY,
        KeypadSubtract = NRE_KEY_KP_SUBTRACT,
        KeypadAdd = NRE_KEY_KP_ADD,
        KeypadEnter = NRE_KEY_KP_ENTER,
        KeypadEqual = NRE_KEY_KP_EQUAL,
        LeftShift = NRE_KEY_LEFT_SHIFT,
        LeftControl = NRE_KEY_LEFT_CONTROL,
        LeftAlt = NRE_KEY_LEFT_ALT,
        LeftSuper = NRE_KEY_LEFT_SUPER,
        RightShift = NRE_KEY_RIGHT_SHIFT,
        RightControl = NRE_KEY_RIGHT_CONTROL,
        RightAlt = NRE_KEY_RIGHT_ALT,
        RightSuper = NRE_KEY_RIGHT_SUPER,
        Menu = NRE_KEY_MENU,
    }
}
