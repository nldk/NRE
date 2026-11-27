package nre;

/** Handle to a pipeline ({@code NrePipeline}). See {@link Handle}. */
public record Pipeline(int index, int generation) implements Handle {
    /** The empty handle. */
    public static final Pipeline NONE = new Pipeline(0, 0);
}
