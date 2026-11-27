package nre;

/** {@code NreApiMode} / {@code NRE::ApiMode}: SceneMode draws the bound scene, FunctionDrawMode draws raylib style (see renderer.h). */
public enum ApiMode {
    SCENE(0),
    FUNCTION_DRAW(1);

    private final int value;

    ApiMode(int value) { this.value = value; }

    /** The C value. */
    public int value() { return value; }
}
