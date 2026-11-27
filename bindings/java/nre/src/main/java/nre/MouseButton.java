package nre;

/** {@code NreMouseButton} / {@code NRE::MouseButton}. */
public enum MouseButton {
    LEFT(0),
    RIGHT(1),
    MIDDLE(2);

    private final int value;

    MouseButton(int value) { this.value = value; }

    /** The C value. */
    public int value() { return value; }
}
