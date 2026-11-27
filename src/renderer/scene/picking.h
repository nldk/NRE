#pragma once
// Ray construction and ray/geometry tests for picking. Header-only and free of Vulkan, so the CPU tests include it.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>

#include <glm/glm.hpp>

#include "renderer.h"

namespace NRE::Picking {
    struct Ray {
        glm::vec3 origin{0.0f};
        glm::vec3 direction{0.0f, 0.0f, -1.0f};   // normalised
    };

    // The ray from the camera through a pixel (top-left origin) of a sceneSize image drawn with projection and view
    // (reverse-Z, infinite far plane, Vulkan Y flip: LightingMath::reverseZPerspective). The pixel is unprojected at
    // the near plane (depth 1), because depth 0 is at infinity.
    inline Ray rayFromPixel(glm::vec2 pixel, glm::vec2 sceneSize, const glm::mat4& projection, const glm::mat4& view) {
        glm::vec2 ndc = pixel / sceneSize * 2.0f - 1.0f;
        glm::mat4 inverseView = glm::inverse(view);
        glm::vec4 nearPoint = glm::inverse(projection) * glm::vec4(ndc, 1.0f, 1.0f);
        glm::vec3 viewPoint = glm::vec3(nearPoint) / nearPoint.w;
        Ray ray;
        ray.origin = glm::vec3(inverseView[3]);
        ray.direction = glm::normalize(glm::vec3(inverseView * glm::vec4(viewPoint, 1.0f)) - ray.origin);
        return ray;
    }

    // Moller-Trumbore, double-sided. t is the distance along direction (in its units) to the hit; only t > 0 counts.
    inline bool rayTriangle(glm::vec3 origin, glm::vec3 direction, glm::vec3 a, glm::vec3 b, glm::vec3 c, float& t) {
        constexpr float epsilon = 1e-9f;
        glm::vec3 e1 = b - a, e2 = c - a;
        glm::vec3 p = glm::cross(direction, e2);
        float det = glm::dot(e1, p);
        if (!(std::fabs(det) > epsilon)) return false;   // parallel, degenerate or NaN
        float inv = 1.0f / det;
        glm::vec3 s = origin - a;
        float u = glm::dot(s, p) * inv;
        if (u < 0.0f || u > 1.0f) return false;
        glm::vec3 q = glm::cross(s, e1);
        float v = glm::dot(direction, q) * inv;
        if (v < 0.0f || u + v > 1.0f) return false;
        float hit = glm::dot(e2, q) * inv;
        if (!(hit > 0.0f)) return false;
        t = hit;
        return true;
    }

    // Slab test: does the ray enter the box (or start inside it) within tMax? Boxes entirely behind the origin miss.
    inline bool rayAabb(glm::vec3 origin, glm::vec3 direction, glm::vec3 lo, glm::vec3 hi, float tMax) {
        float tNear = 0.0f, tFar = tMax;
        for (int axis = 0; axis < 3; ++axis) {
            if (direction[axis] == 0.0f) {
                if (origin[axis] < lo[axis] || origin[axis] > hi[axis]) return false;
                continue;
            }
            float inv = 1.0f / direction[axis];
            float t0 = (lo[axis] - origin[axis]) * inv;
            float t1 = (hi[axis] - origin[axis]) * inv;
            if (t0 > t1) std::swap(t0, t1);
            tNear = std::max(tNear, t0);
            tFar = std::min(tFar, t1);
            if (tNear > tFar) return false;
        }
        return true;
    }

    // Nearest hit of the ray against an indexed triangle list (V has .position). Incomplete triangles and indices
    // out of range are skipped. t is only written on a hit.
    template <class V>
    bool rayMesh(glm::vec3 origin, glm::vec3 direction, std::span<const V> vertices, std::span<const uint32_t> indices, float& t) {
        bool hit = false;
        float best = 0.0f;
        for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
            if (indices[i] >= vertices.size() || indices[i + 1] >= vertices.size() || indices[i + 2] >= vertices.size()) continue;
            float distance = 0.0f;
            if (rayTriangle(origin, direction, vertices[indices[i]].position, vertices[indices[i + 1]].position,
                            vertices[indices[i + 2]].position, distance) && (!hit || distance < best)) {
                best = distance;
                hit = true;
            }
        }
        if (hit) t = best;
        return hit;
    }
}

namespace NRE {
    class Scene;
    namespace Picking {
        // The nearest hit of a world-space ray against the scene's visible instances whose model is alive. The
        // distance is along the (normalised) ray in world units. Back faces count.
        std::optional<RayHit> pickScene(const Scene& scene, const Ray& ray);
    }
}
