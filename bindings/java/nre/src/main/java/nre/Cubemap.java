package nre;

/** Handle to a cubemap ({@code NreCubemap}). See {@link Handle}. */
public record Cubemap(int index, int generation) implements Handle {
    /** The empty handle. */
    public static final Cubemap NONE = new Cubemap(0, 0);
}
