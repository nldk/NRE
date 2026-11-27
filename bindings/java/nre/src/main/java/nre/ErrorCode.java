package nre;

/**
 * {@code NreErrorCode}: what went wrong in a failed call. The values are the C ones and never change.
 * Integers this version doesn't know (from a newer library) map to {@link #UNKNOWN}; see {@link #fromValue}.
 */
public enum ErrorCode {
    OK(0),
    UNSUPPORTED_GPU(1),
    CREATING_VULKAN_INSTANCE(2),
    CREATING_SWAPCHAIN(3),
    RECREATING_SWAPCHAIN(4),
    CREATING_PIPELINE(5),
    WINDOW_CREATION_FAILED(6),
    FILE_NOT_FOUND(7),
    WRONG_MODE(8),
    NOT_INITIALIZED(9),
    ALREADY_INITIALIZED(10),
    /** Empty, destroyed, or never created. */
    INVALID_HANDLE(11),
    UNSUPPORTED_FORMAT(12),
    MODEL_LOAD_FAILED(13),
    FAILED_TO_CREATE_ALLOCATOR(14),
    BUFFER_CREATION_FAILED(15),
    UPLOAD_FAILED(16),
    NO_PIPELINE_BOUND(17),
    NO_SCENE_BOUND(18),
    CREATING_TEXTURE(19),
    IMAGE_CREATION_FAILED(20),
    IMAGE_VIEW_CREATION_FAILED(21),
    INDEX_OUT_OF_RANGE(22),
    IMPOSSIBLE_TYPE_FOR_PER_FRAME_BUFFER(23),
    BUFFER_WITH_SIZE_NULL(24),
    SIZE_OF_BUFFER_TOO_BIG(25),
    DESCRIPTOR_CREATION_FAILED(26),
    SHADER_CREATION_FAILED(27),
    /** Also: a C++ exception inside the renderer, or a code this version doesn't know. */
    UNKNOWN(28),
    LIBRARY_LOAD_FAILED(29),
    /** E.g. a negative shadow distance, or picking in RENDER_2D. */
    INVALID_ARGUMENT(30);

    private static final ErrorCode[] BY_VALUE = values(); // ordinal == value

    private final int value;

    ErrorCode(int value) { this.value = value; }

    /** The C value. */
    public int value() { return value; }

    /** The code for a C value; values this version doesn't know give {@link #UNKNOWN}. */
    public static ErrorCode fromValue(int value) {
        return value >= 0 && value < BY_VALUE.length ? BY_VALUE[value] : UNKNOWN;
    }
}
