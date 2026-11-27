#include "scene/picking.h"
#include "lighting/lightingMath.h"

#include <cmath>
#include <cstdio>
#include <vector>

static bool g_failed = false;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            g_failed = true; \
        } \
    } while (0)

#define CHECK_NEAR(a, b, eps) CHECK(std::fabs(double(a) - double(b)) <= double(eps))

using namespace NRE::Picking;

static const glm::vec3 A{-1, -1, -5}, B{1, -1, -5}, C{0, 1, -5};

static void triangleHit() {
    float t = 0;
    CHECK(rayTriangle({0, 0, 0}, {0, 0, -1}, A, B, C, t));
    CHECK_NEAR(t, 5.0f, 1e-5);
    CHECK(rayTriangle({0, 0, -10}, {0, 0, 1}, A, B, C, t));   // from behind: back faces count
    CHECK_NEAR(t, 5.0f, 1e-5);
}

static void triangleMiss() {
    float t = 0;
    CHECK(!rayTriangle({3, 0, 0}, {0, 0, -1}, A, B, C, t));
    CHECK(!rayTriangle({0, 0, 0}, {0, 0, 1}, A, B, C, t));       // triangle behind the origin
    CHECK(!rayTriangle({0, 0, -10}, {0, 0, -1}, A, B, C, t));    // moving away from it
}

static void triangleEdgeAndParallel() {
    float t = 0;
    CHECK(rayTriangle({0, -1, 0}, {0, 0, -1}, A, B, C, t));      // on the bottom edge
    CHECK(rayTriangle({-1, -1, 0}, {0, 0, -1}, A, B, C, t));     // on a vertex
    CHECK(!rayTriangle({0, 0, 0}, {1, 0, 0}, A, B, C, t));       // parallel to the plane
    CHECK(!rayTriangle({0, 0, -5}, {1, 0, 0}, A, B, C, t));      // in the plane
}

static void triangleDegenerate() {
    float t = 0;
    CHECK(!rayTriangle({0, 0, 0}, {0, 0, -1}, A, A, B, t));
    CHECK(!rayTriangle({0, 0, 0}, {0, 0, -1}, A, {0, -1, -5}, {2, -1, -5}, t));   // collinear
}

static void aabb() {
    glm::vec3 lo{-1, -1, -1}, hi{1, 1, 1};
    CHECK(rayAabb({0, 0, 5}, {0, 0, -1}, lo, hi, 100.0f));
    CHECK(!rayAabb({3, 0, 5}, {0, 0, -1}, lo, hi, 100.0f));
    CHECK(!rayAabb({0, 0, 5}, {0, 0, 1}, lo, hi, 100.0f));      // box behind the origin
    CHECK(rayAabb({0, 0, 0}, {1, 0, 0}, lo, hi, 100.0f));       // origin inside
    CHECK(rayAabb({0, 0, 5}, {0, 0, -1}, lo, hi, 4.5f));        // entry at 4: within tmax
    CHECK(!rayAabb({0, 0, 5}, {0, 0, -1}, lo, hi, 3.0f));       // entry at 4: too far
    CHECK(!rayAabb({0, 2, 5}, {0, 0, -1}, lo, hi, 100.0f));     // axis-parallel ray outside the slab
}

struct V { glm::vec3 position; };

static void mesh() {
    std::vector<V> v{{{-1, -1, -5}}, {{1, -1, -5}}, {{0, 1, -5}}, {{-1, -1, -3}}, {{1, -1, -3}}, {{0, 1, -3}}};
    std::vector<uint32_t> i{0, 1, 2, 3, 4, 5};
    float t = 0;
    CHECK(rayMesh<V>({0, 0, 0}, {0, 0, -1}, v, i, t));
    CHECK_NEAR(t, 3.0f, 1e-5);                                   // the nearer triangle wins
    CHECK(!rayMesh<V>({0, 0, 0}, {0, 0, -1}, v, {}, t));         // no indices
    std::vector<uint32_t> bad{0, 1, 99};
    CHECK(!rayMesh<V>({0, 0, 0}, {0, 0, -1}, v, bad, t));        // out of range index
    std::vector<uint32_t> partial{0, 1};
    CHECK(!rayMesh<V>({0, 0, 0}, {0, 0, -1}, v, partial, t));    // incomplete triangle
}

static void pixelRay() {
    glm::mat4 P = NRE::LightingMath::reverseZPerspective(glm::radians(60.0f), 4.0f / 3.0f, 0.1f);
    glm::mat4 view = glm::lookAt(glm::vec3(1, 2, 3), glm::vec3(1, 2, 2), glm::vec3(0, 1, 0));
    Ray centre = rayFromPixel({320, 240}, {640, 480}, P, view);
    CHECK_NEAR(centre.origin.x, 1, 1e-4); CHECK_NEAR(centre.origin.y, 2, 1e-4); CHECK_NEAR(centre.origin.z, 3, 1e-4);
    CHECK_NEAR(centre.direction.z, -1, 1e-4);
    Ray topLeft = rayFromPixel({0, 0}, {640, 480}, P, view);
    CHECK(topLeft.direction.x < 0 && topLeft.direction.y > 0);   // up and left of the view
    CHECK_NEAR(glm::length(topLeft.direction), 1, 1e-5);
    CHECK_NEAR(topLeft.direction.y / -topLeft.direction.z, std::tan(glm::radians(30.0f)), 1e-3);
    CHECK_NEAR(topLeft.direction.x / -topLeft.direction.z, -std::tan(glm::radians(30.0f)) * 4.0f / 3.0f, 1e-3);
}

int main() {
    triangleHit();
    triangleMiss();
    triangleEdgeAndParallel();
    triangleDegenerate();
    aabb();
    mesh();
    pixelRay();
    if (g_failed) return 1;
    std::printf("picking tests passed\n");
    return 0;
}
