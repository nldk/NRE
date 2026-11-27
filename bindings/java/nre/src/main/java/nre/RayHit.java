package nre;

/**
 * What {@link NRE#pickInstance} hit, like {@code NreRayHit} / {@code NRE::RayHit}: the instance, the distance from
 * the camera along the ray (world units) and the world-space point.
 */
public record RayHit(Instance instance, float distance, Vec3 point) {}
