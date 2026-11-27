package nre;

/** {@code NreRenderMode} / {@code NRE::RenderMode}. */
public enum RenderMode {
    RENDER_3D(0),
    RENDER_2D(1);

    private final int value;

    RenderMode(int value) { this.value = value; }

    /** The C value. */
    public int value() { return value; }
}
