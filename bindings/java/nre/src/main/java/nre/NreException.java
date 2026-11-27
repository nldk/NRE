package nre;

/**
 * Thrown when an NRE call returns an error. Carries the {@link ErrorCode} and the message from
 * {@code nreGetLastErrorMessage()}, read right after the failing call on the same thread.
 * {@link #getMessage()} is {@code "CODE: message"}; {@link #nreMessage()} is the library's message alone.
 */
public final class NreException extends RuntimeException {
    private static final long serialVersionUID = 1L;

    private final ErrorCode code;
    private final int rawCode;
    private final String nreMessage;

    public NreException(int rawCode, String nreMessage) {
        super(describe(rawCode) + ": " + nreMessage);
        this.code = ErrorCode.fromValue(rawCode);
        this.rawCode = rawCode;
        this.nreMessage = nreMessage;
    }

    private static String describe(int rawCode) {
        ErrorCode code = ErrorCode.fromValue(rawCode);
        return code.value() == rawCode ? code.name() : code.name() + " (" + rawCode + ")";
    }

    /** The error code; {@link ErrorCode#UNKNOWN} for a value this version doesn't know. */
    public ErrorCode code() { return code; }

    /** The integer the C function returned, useful for codes newer than these bindings. */
    public int rawCode() { return rawCode; }

    /** The message from {@code nreGetLastErrorMessage()}. */
    public String nreMessage() { return nreMessage; }
}
