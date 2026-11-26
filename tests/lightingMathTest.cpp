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

static void cascadeCount() {
    CHECK(cascadeCountFor(20) == 1);
    CHECK(cascadeCountFor(30) == 1);
    CHECK(cascadeCountFor(100) == 2);
    CHECK(cascadeCountFor(300) == 3);
    CHECK(cascadeCountFor(500) == 4);
    CHECK(cascadeCountFor(0) == 0);
}

static bool allFinite(const glm::mat4& m) {
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            if (!std::isfinite(m[c][r])) return false;
    return true;
}

static void cascadeViewProjVerticalSun() {
    glm::mat4 view = glm::lookAt(glm::vec3(0, 5, 0), glm::vec3(0, 5, -1), glm::vec3(0, 1, 0));
    auto corners = frustumSliceCorners(view, glm::radians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);
    glm::mat4 m = cascadeViewProj(corners, glm::vec3(0, -1, 0), 2048);
    CHECK(allFinite(m));
    for (const glm::vec3& c : corners) {
        glm::vec4 p = m * glm::vec4(c, 1);
        p /= p.w;
        CHECK(p.x >= -1.0001f && p.x <= 1.0001f && p.y >= -1.0001f && p.y <= 1.0001f);
        CHECK(p.z >= 0.0f && p.z <= 1.0f);
    }
}

static void cascadeSplitsIncreasing() {
    for (uint32_t count : {1u, 2u, 3u, 4u}) {
        auto s = cascadeSplits(0.1f, 500.0f, count);
        for (uint32_t i = 1; i < count; ++i) CHECK(s[i] > s[i - 1]);
        CHECK(s[0] > 0.1f);
        CHECK_NEAR(s[count - 1], 500.0f, 1e-3);
        for (uint32_t i = count; i < 4; ++i) CHECK_NEAR(s[i], 500.0f, 1e-3);
    }
}

static void cascadeStable() {
    const glm::vec3 sun = glm::normalize(glm::vec3(0.3f, -0.8f, 0.4f));
    const uint32_t res = 2048;
    auto build = [&](glm::vec3 eye) {
        glm::mat4 view = glm::lookAt(eye, eye + glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));
        auto corners = frustumSliceCorners(view, glm::radians(60.0f), 16.0f / 9.0f, 0.1f, 80.0f);
        return cascadeViewProj(corners, sun, res);
    };
    const glm::vec3 eye0(10.0f, 3.0f, 20.0f);
    glm::mat4 a = build(eye0);
    // World size of one shadow texel: row 0 of the matrix is (light right) / radius, so ndc spans 2 / length(row0)
    // world units over `res` texels.
    glm::vec3 lightRight = glm::normalize(glm::cross(sun, glm::vec3(0, 1, 0)));
    glm::vec3 lightUp = glm::cross(lightRight, sun);
    float texel = 2.0f / (glm::length(glm::vec3(a[0][0], a[1][0], a[2][0])) * float(res));
    // Move 0.3 texel along both light axes so both snaps are exercised.
    glm::mat4 b = build(eye0 + (lightRight + lightUp) * (0.3f * texel));
    for (int axis = 0; axis < 2; ++axis) {
        double ta = double((a * glm::vec4(0, 0, 0, 1))[axis]) * res * 0.5;
        double tb = double((b * glm::vec4(0, 0, 0, 1))[axis]) * res * 0.5;
        // Each projected world origin sits on a whole texel (the snap invariant), so their difference is whole too.
        CHECK(std::fabs(ta - std::round(ta)) < 1e-3);
        CHECK(std::fabs(tb - std::round(tb)) < 1e-3);
        double d = ta - tb;
        CHECK(std::fabs(d - std::round(d)) < 1e-3);
    }
}

// The shadow pass relies on depth clamping: casters sunward of the tight near plane project in front of it (z < 0)
// but stay inside the map in xy.
static void cascadeCasterClampsToNearPlane() {
    const glm::vec3 sun = glm::normalize(glm::vec3(0.3f, -0.8f, 0.4f));
    glm::mat4 view = glm::lookAt(glm::vec3(10, 3, 20), glm::vec3(10, 3, 19), glm::vec3(0, 1, 0));
    auto corners = frustumSliceCorners(view, glm::radians(60.0f), 16.0f / 9.0f, 0.1f, 20.0f);
    glm::mat4 m = cascadeViewProj(corners, sun, 2048);
    glm::vec3 centre(0.0f);
    for (const glm::vec3& c : corners) centre += c;
    centre /= 8.0f;
    glm::vec4 p = m * glm::vec4(centre - sun * 100.0f, 1.0f);
    CHECK(p.w == 1.0f);
    CHECK(p.z < 0.0f);
    CHECK(std::fabs(p.x) <= 1.0f && std::fabs(p.y) <= 1.0f);
}

static void transmittanceZenithBrighter() {
    glm::vec3 zenith = sunTransmittance(glm::radians(90.0f));
    glm::vec3 low = sunTransmittance(glm::radians(2.0f));
    CHECK(zenith.x > low.x && zenith.y > low.y && zenith.z > low.z);
}

static void sunsetRed() {
    glm::vec3 t = sunTransmittance(glm::radians(2.0f));
    CHECK(t.z > 0.0f && t.x / t.z > 3.0f);
}

static void autoExposureShape() {
    CHECK_NEAR(autoExposure(glm::radians(30.0f)), 1.0, 1e-5);
    CHECK_NEAR(autoExposure(glm::radians(60.0f)), 1.0, 1e-5);
    CHECK_NEAR(autoExposure(glm::radians(90.0f)), 1.0, 1e-5);
    float prev = autoExposure(glm::radians(-90.0f));
    for (float deg = -89.0f; deg <= 90.0f; deg += 1.0f) {
        float e = autoExposure(glm::radians(deg));
        CHECK(e <= prev + 1e-6f);
        prev = e;
    }
    float n10 = autoExposure(glm::radians(-10.0f));
    float n45 = autoExposure(glm::radians(-45.0f));
    CHECK(std::isfinite(n10) && n10 > 1.0f);
    CHECK_NEAR(n10, n45, 1e-5);
}

static void sunFadesSmoothly() {
    auto T = [](float deg) { return sunTransmittance(glm::radians(deg)); };
    glm::vec3 at0 = T(0.0f);
    CHECK(at0.x > 0.0f);
    glm::vec3 prev = T(2.0f);
    for (int i = 1; i <= 80; ++i) {
        float deg = 2.0f - 0.05f * float(i);
        glm::vec3 t = T(deg);
        CHECK(std::isfinite(t.x) && std::isfinite(t.y) && std::isfinite(t.z));
        CHECK(t.x <= prev.x + 1e-7f && t.y <= prev.y + 1e-7f && t.z <= prev.z + 1e-7f);
        CHECK(prev.x - t.x <= 0.1f * at0.x); // red carries the light here; g/b fall steeply by physics above 0 deg
        prev = t;
    }
    glm::vec3 low = T(-1.0f);
    CHECK(low.x == 0.0f && low.y == 0.0f && low.z == 0.0f);
    glm::vec3 deep = T(-30.0f);
    CHECK(deep.x == 0.0f && deep.z == 0.0f);
}

static void hazeVisibility() {
    CHECK_NEAR(std::exp(-hazeDensityForVisibility(8000.0f) * 8000.0f), 0.05, 1e-4);
    CHECK(hazeDensityForVisibility(0.0f) == 0.0f);
}

int main() {
    toneMapRoundTrip();
    inverseToneMapClamps();
    reverseZDepth();
    reverseZOrthoDepth();
    cascadeCount();
    cascadeViewProjVerticalSun();
    cascadeSplitsIncreasing();
    cascadeStable();
    cascadeCasterClampsToNearPlane();
    transmittanceZenithBrighter();
    sunsetRed();
    autoExposureShape();
    sunFadesSmoothly();
    hazeVisibility();
    return g_failed ? 1 : 0;
}
