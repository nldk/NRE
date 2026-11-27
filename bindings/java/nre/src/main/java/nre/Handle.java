package nre;

/**
 * An NRE handle: a small value (index + generation) to copy around, like the C {@code NreModel} etc.
 * Don't read or change the fields; {@code index} is an unsigned 32-bit value stored in an int.
 * The all-zero handle ({@code NONE} on each type) is the empty handle, e.g. "no texture" or "no skybox".
 * Handles are destroyed explicitly with the matching {@code NRE.destroyXxx}; nothing is freed by the GC.
 */
public sealed interface Handle
        permits Shader, Pipeline, Model, Instance, Light, Scene, Texture, Material, Cubemap {
    int index();
    int generation();

    /** True for the empty handle. */
    default boolean isNone() { return index() == 0 && generation() == 0; }
}
