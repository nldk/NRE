package nre;

/** Handle to a texture ({@code NreTexture}). See {@link Handle}. */
public record Texture(int index, int generation) implements Handle {
    /** The empty handle. */
    public static final Texture NONE = new Texture(0, 0);
}
