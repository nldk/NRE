package nre;

/** {@code NreShaderType} / {@code NRE::ShaderType}. */
public enum ShaderType {
    FRAGMENT(0),
    VERTEX(1),
    GEOMETRY(2),
    COMPUTE(3);

    private final int value;

    ShaderType(int value) { this.value = value; }

    /** The C value. */
    public int value() { return value; }
}
