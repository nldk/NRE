package nre;

/** Handle to a instance ({@code NreInstance}). See {@link Handle}. */
public record Instance(int index, int generation) implements Handle {
    /** The empty handle. */
    public static final Instance NONE = new Instance(0, 0);
}
