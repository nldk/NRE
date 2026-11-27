package nre;

/** Handle to a scene ({@code NreScene}). See {@link Handle}. */
public record Scene(int index, int generation) implements Handle {
    /** The empty handle. */
    public static final Scene NONE = new Scene(0, 0);
}
