package nre;

/** Handle to a model ({@code NreModel}). See {@link Handle}. */
public record Model(int index, int generation) implements Handle {
    /** The empty handle. */
    public static final Model NONE = new Model(0, 0);
}
