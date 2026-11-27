package nre;

/** Handle to a shader ({@code NreShader}). See {@link Handle}. */
public record Shader(int index, int generation) implements Handle {
    /** The empty handle. */
    public static final Shader NONE = new Shader(0, 0);
}
