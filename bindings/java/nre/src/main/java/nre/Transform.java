package nre;

import java.util.Objects;

/**
 * Position, rotation and scale of an object in world space, like {@code NRE::Transform}.
 * Applied as scale, then rotation, then translation (see renderer.h).
 */
public record Transform(Vec3 position, Quat rotation, Vec3 scale) {
    public static final Transform IDENTITY = new Transform(Vec3.ZERO, Quat.IDENTITY, Vec3.ONE);

    public Transform {
        Objects.requireNonNull(position, "position");
        Objects.requireNonNull(rotation, "rotation");
        Objects.requireNonNull(scale, "scale");
    }

    public Transform withPosition(Vec3 position) { return new Transform(position, rotation, scale); }
    public Transform withRotation(Quat rotation) { return new Transform(position, rotation, scale); }
    public Transform withScale(Vec3 scale) { return new Transform(position, rotation, scale); }
}
