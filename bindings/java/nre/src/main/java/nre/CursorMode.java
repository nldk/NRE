package nre;

/** {@code NreCursorMode} / {@code NRE::CursorMode}. HIDDEN: invisible over the window but moves as usual. LOCKED: hidden and kept in the window, for mouse-look cameras. */
public enum CursorMode {
    NORMAL(0),
    HIDDEN(1),
    LOCKED(2);

    private final int value;

    CursorMode(int value) { this.value = value; }

    /** The C value. */
    public int value() { return value; }
}
