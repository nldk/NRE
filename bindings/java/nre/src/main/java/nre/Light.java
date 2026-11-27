package nre;

/** Handle to a light ({@code NreLight}). See {@link Handle}. */
public record Light(int index, int generation) implements Handle {
    /** The empty handle. */
    public static final Light NONE = new Light(0, 0);
}
