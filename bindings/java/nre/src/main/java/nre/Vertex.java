package nre;

import java.util.Objects;

/**
 * One vertex of a model you build yourself ({@link NRE#createMeshModel}), like {@code NreVertex} /
 * {@code NRE::Vertex}. A normal of {@link Vec3#ZERO} is computed from the triangles that use the vertex.
 */
public record Vertex(Vec3 position, Vec3 normal, Vec2 uv) {
    public Vertex {
        Objects.requireNonNull(position, "position");
        Objects.requireNonNull(normal, "normal");
        Objects.requireNonNull(uv, "uv");
    }

    /** Normal computed from the triangles, uv (0, 0). */
    public Vertex(Vec3 position) { this(position, Vec3.ZERO, Vec2.ZERO); }

    /** Normal computed from the triangles. */
    public Vertex(Vec3 position, Vec2 uv) { this(position, Vec3.ZERO, uv); }
}
