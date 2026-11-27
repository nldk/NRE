package nre;

/** The library version from {@code nreGetVersion}. */
public record Version(int major, int minor, int patch) {
    @Override public String toString() { return major + "." + minor + "." + patch; }
}
