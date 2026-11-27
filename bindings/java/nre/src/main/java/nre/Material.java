package nre;

/** Handle to a material ({@code NreMaterial}). See {@link Handle}. */
public record Material(int index, int generation) implements Handle {
    /** The empty handle. */
    public static final Material NONE = new Material(0, 0);
}
