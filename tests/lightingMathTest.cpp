#include "lighting/lightingMath.h"

#include <cmath>
#include <cstdio>

static bool g_failed = false;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            g_failed = true; \
        } \
    } while (0)

#define CHECK_NEAR(a, b, eps) CHECK(std::fabs(double(a) - double(b)) <= double(eps))

using namespace NRE::LightingMath;

static void toneMapRoundTrip() {
    for (float y : {0.0f, 0.05f, 0.2f, 0.5f, 0.8f, 0.95f})
        CHECK_NEAR(toneMapACES(inverseToneMapACES(y)), y, 1e-4);
}

static void inverseToneMapClamps() {
    CHECK(std::isfinite(inverseToneMapACES(1.0f)) && inverseToneMapACES(1.0f) > 5.0f);
}

static void reverseZDepth() {
    glm::mat4 P = reverseZPerspective(glm::radians(60.0f), 16.0f / 9.0f, 0.1f);
    auto depth = [&](float z) {
        glm::vec4 c = P * glm::vec4(0, 0, -z, 1);
        return c.z / c.w;
    };
    CHECK_NEAR(depth(0.1f), 1, 1e-5);
    CHECK(depth(1e6f) > 0 && depth(1e6f) < 1e-6);
    CHECK(depth(10) > depth(100));
    CHECK(P[1][1] < 0); // Vulkan Y flip
}

static void reverseZOrthoDepth() {
    glm::mat4 O = reverseZOrtho(800, 600, 1000);
    auto depth = [&](float z) {
        glm::vec4 c = O * glm::vec4(0, 0, z, 1);
        return c.z / c.w;
    };
    CHECK(depth(999.0f) > depth(-999.0f)); // toward the viewer = greater depth
}

int main() {
    toneMapRoundTrip();
    inverseToneMapClamps();
    reverseZDepth();
    reverseZOrthoDepth();
    return g_failed ? 1 : 0;
}
