package nre;

/** A rotation quaternion, like {@code NreQuat}. Field order x, y, z, w; identity is (0, 0, 0, 1). */
public record Quat(float x, float y, float z, float w) {
    public static final Quat IDENTITY = new Quat(0, 0, 0, 1);
}
