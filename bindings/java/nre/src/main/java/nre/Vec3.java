package nre;

/** Three floats, like {@code NreVec3} / {@code glm::vec3}. */
public record Vec3(float x, float y, float z) {
    public static final Vec3 ZERO = new Vec3(0, 0, 0);
    public static final Vec3 ONE = new Vec3(1, 1, 1);
    /** World up, +Y (the camera's up). */
    public static final Vec3 UP = new Vec3(0, 1, 0);
    /** +X: the camera's right at yaw 0. */
    public static final Vec3 RIGHT = new Vec3(1, 0, 0);
    /** -Z: the camera's forward at yaw 0 and pitch 0 (right-handed, like OpenGL/glm). */
    public static final Vec3 FORWARD = new Vec3(0, 0, -1);

    public Vec3 add(Vec3 o) { return new Vec3(x + o.x, y + o.y, z + o.z); }
    public Vec3 sub(Vec3 o) { return new Vec3(x - o.x, y - o.y, z - o.z); }
    public Vec3 scale(float s) { return new Vec3(x * s, y * s, z * s); }
}
