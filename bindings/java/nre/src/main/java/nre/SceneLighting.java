package nre;

/**
 * {@code NreSceneLighting} / {@code NRE::SceneLighting}: the bound scene's lighting mode ({@link NRE#setSceneLighting}).
 * The values are the C ones and never change; see renderer.h.
 */
public enum SceneLighting {
    /** The default: the cubemap or the sky colour, short sharp shadows (20 m), no haze, exposure as set. */
    SMALL(0),
    /** Procedural sky from the sun, cascaded shadows reaching far (500 m), haze, automatic exposure. */
    LANDSCAPE(1);

    private static final SceneLighting[] BY_VALUE = values(); // ordinal == value

    private final int value;

    SceneLighting(int value) { this.value = value; }

    /** The C value. */
    public int value() { return value; }

    /**
     * The mode for a C value. Throws IllegalArgumentException for a value this version doesn't know (only a newer
     * library could return one, and the version check rejects those).
     */
    public static SceneLighting fromValue(int value) {
        if (value >= 0 && value < BY_VALUE.length) return BY_VALUE[value];
        throw new IllegalArgumentException("NRE: unknown NreSceneLighting value " + value);
    }
}
