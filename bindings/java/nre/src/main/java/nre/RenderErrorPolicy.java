package nre;

/** {@code NreRenderErrorPolicy} / {@code NRE::RenderErrorPolicy}: what the frame functions do with errors (see renderer.h). */
public enum RenderErrorPolicy {
    IGNORE(0),
    RETURN(1),
    PANIC(2);

    private final int value;

    RenderErrorPolicy(int value) { this.value = value; }

    /** The C value. */
    public int value() { return value; }
}
