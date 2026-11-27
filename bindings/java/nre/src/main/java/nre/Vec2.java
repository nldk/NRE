package nre;

/** Two floats, like {@code NreVec2} / {@code glm::vec2}. */
public record Vec2(float x, float y) {
    public static final Vec2 ZERO = new Vec2(0, 0);

    public Vec2 add(Vec2 o) { return new Vec2(x + o.x, y + o.y); }
    public Vec2 sub(Vec2 o) { return new Vec2(x - o.x, y - o.y); }
    public Vec2 scale(float s) { return new Vec2(x * s, y * s); }
}
