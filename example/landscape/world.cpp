#include "world.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>

#include <glm/gtc/noise.hpp>

namespace Landscape {
    namespace {
        // Palette layout (keep in sync with makePalette.py).
        constexpr float paletteMinHeight = -30.0f, paletteMaxHeight = 700.0f;
        constexpr float terrainRows = 192.0f;              // palette rows used by the terrain colours
        constexpr float paletteHeight = 256.0f;
        glm::vec2 swatch(int column, int row) { return {(column + 0.5f) / 4.0f, (row + 16) / 256.0f}; }
        const glm::vec2 darkFoliage = swatch(0, 192), lightFoliage = swatch(1, 192), bark = swatch(2, 192);
        const glm::vec2 rockGrey = swatch(3, 192), water = swatch(0, 224);

        // Fractal noise: octaves of simplex noise, each twice the frequency and half the strength.
        float fbm(glm::vec2 p, int octaves) {
            float sum = 0.0f, amplitude = 1.0f, norm = 0.0f;
            for (int i = 0; i < octaves; ++i) {
                sum += amplitude * glm::simplex(p);
                norm += amplitude;
                p = p * 2.03f + glm::vec2(17.1f, -9.3f);
                amplitude *= 0.5f;
            }
            return sum / norm;
        }
        // Sharp crests instead of round hills: 1 - |noise|, squared.
        float ridged(glm::vec2 p, int octaves) {
            float sum = 0.0f, amplitude = 1.0f, norm = 0.0f;
            for (int i = 0; i < octaves; ++i) {
                float r = 1.0f - std::fabs(glm::simplex(p));
                sum += amplitude * r * r;
                norm += amplitude;
                p = p * 2.07f + glm::vec2(-5.7f, 11.9f);
                amplitude *= 0.5f;
            }
            return sum / norm;
        }

        glm::vec3 terrainNormal(float x, float z) {
            constexpr float e = 2.0f;
            float dx = terrainHeight(x + e, z) - terrainHeight(x - e, z);
            float dz = terrainHeight(x, z + e) - terrainHeight(x, z - e);
            return glm::normalize(glm::vec3(-dx, 2.0f * e, -dz));
        }

        // Adds a ring of `sides` vertices at height y with radius r; returns the index of the first one.
        uint32_t ring(MeshData& m, int sides, float y, float r, glm::vec2 uv, float outward) {
            uint32_t first = uint32_t(m.vertices.size());
            for (int i = 0; i < sides; ++i) {
                float a = 2.0f * std::numbers::pi_v<float> * i / sides;
                glm::vec3 dir(std::cos(a), 0.0f, std::sin(a));
                glm::vec3 normal = glm::normalize(dir + glm::vec3(0.0f, outward, 0.0f));
                m.vertices.push_back({glm::vec3(dir.x * r, y, dir.z * r), normal, uv});
            }
            return first;
        }
        // Side wall between two rings (counter-clockwise seen from outside).
        void wall(MeshData& m, uint32_t lower, uint32_t upper, int sides) {
            for (int i = 0; i < sides; ++i) {
                uint32_t a = lower + i, b = lower + (i + 1) % sides, c = upper + i, d = upper + (i + 1) % sides;
                m.indices.insert(m.indices.end(), {a, c, b, b, c, d});
            }
        }
        // A cone from a ring up to a tip vertex.
        void cone(MeshData& m, float baseY, float radius, float height, glm::vec2 uv) {
            constexpr int sides = 8;
            uint32_t base = ring(m, sides, baseY, radius, uv, radius / height);
            uint32_t tip = uint32_t(m.vertices.size());
            m.vertices.push_back({glm::vec3(0.0f, baseY + height, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), uv});
            for (int i = 0; i < sides; ++i) {
                uint32_t a = base + i, b = base + (i + 1) % sides;
                m.indices.insert(m.indices.end(), {a, tip, b});
            }
            // Underside, so the crown doesn't look hollow from below.
            uint32_t centre = uint32_t(m.vertices.size());
            m.vertices.push_back({glm::vec3(0.0f, baseY, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), uv});
            for (int i = 0; i < sides; ++i) {
                uint32_t a = base + i, b = base + (i + 1) % sides;
                m.indices.insert(m.indices.end(), {a, b, centre});
            }
        }

        std::vector<Placement> place(int count, uint32_t seed, bool (*fits)(float x, float z, float h, float slope, float r)) {
            std::mt19937 rng(seed);
            std::uniform_real_distribution<float> coord(-worldSize * 0.48f, worldSize * 0.48f), unit(0.0f, 1.0f);
            std::vector<Placement> out;
            for (int tries = 0; int(out.size()) < count && tries < count * 50; ++tries) {
                float x = coord(rng), z = coord(rng);
                float h = groundHeight(x, z);
                if (!fits(x, z, h, terrainSlope(x, z), unit(rng))) continue;
                out.push_back({glm::vec3(x, h - 0.3f, z), unit(rng) * 2.0f * std::numbers::pi_v<float>,
                               0.7f + 0.6f * unit(rng)});
            }
            return out;
        }
    }

    float terrainHeight(float x, float z) {
        glm::vec2 p(x, z);
        // Rolling hills everywhere.
        float h = 60.0f * fbm(p / 900.0f, 5) + 35.0f;
        // A mountain range across the north (−z), fading out toward the south.
        float north = std::clamp((-z + 200.0f) / 1600.0f, 0.0f, 1.0f);
        h += 620.0f * north * north * ridged(p / 1300.0f + glm::vec2(3.7f, 1.3f), 6);
        // A lake basin south-east of the centre.
        // Steep banks: flat bottom, then a quick rise near the rim, so the shore isn't one wide beach.
        float lake = glm::length(p - glm::vec2(500.0f, 700.0f)) / 520.0f;
        float bank = std::clamp((1.0f - lake) / 0.25f, 0.0f, 1.0f);
        h -= 90.0f * bank * bank * (3.0f - 2.0f * bank);
        return h;
    }

    float groundHeight(float x, float z) {
        constexpr float step = worldSize / float(terrainResolution - 1);
        float gx = std::clamp((x + worldSize * 0.5f) / step, 0.0f, float(terrainResolution - 1) - 1e-3f);
        float gz = std::clamp((z + worldSize * 0.5f) / step, 0.0f, float(terrainResolution - 1) - 1e-3f);
        float i = std::floor(gx), j = std::floor(gz), fx = gx - i, fz = gz - j;
        auto at = [](float i, float j) {
            return terrainHeight(-worldSize * 0.5f + i * step, -worldSize * 0.5f + j * step);
        };
        float a = at(i, j), b = at(i + 1, j), c = at(i, j + 1), d = at(i + 1, j + 1);
        // makeTerrain splits each cell along the b–c diagonal: triangles (a, c, b) and (b, c, d).
        if (fx + fz <= 1.0f) return a + fx * (b - a) + fz * (c - a);
        return d + (1.0f - fx) * (c - d) + (1.0f - fz) * (b - d);
    }

    float terrainSlope(float x, float z) {
        return 1.0f - terrainNormal(x, z).y;
    }

    MeshData makeTerrain() {
        constexpr int resolution = terrainResolution;
        MeshData m;
        m.vertices.reserve(size_t(resolution) * resolution);
        const float step = worldSize / float(resolution - 1);
        for (int j = 0; j < resolution; ++j) {
            for (int i = 0; i < resolution; ++i) {
                float x = -worldSize * 0.5f + i * step, z = -worldSize * 0.5f + j * step;
                float h = terrainHeight(x, z);
                glm::vec3 normal = terrainNormal(x, z);
                float u = std::clamp((h - paletteMinHeight) / (paletteMaxHeight - paletteMinHeight), 0.002f, 0.998f);
                float steep = std::min(std::clamp(1.0f - normal.y, 0.0f, 1.0f) * 2.0f, 1.0f);   // ≥ 60°: cliff
                // Texel centres of rows 0..191 only: v = 0 would blend in row 255 (the sampler repeats).
                float v = (0.5f + steep * (terrainRows - 1.0f)) / paletteHeight;
                m.vertices.push_back({glm::vec3(x, h, z), normal, glm::vec2(u, v)});
            }
        }
        m.indices.reserve(size_t(resolution - 1) * (resolution - 1) * 6);
        for (int j = 0; j + 1 < resolution; ++j) {
            for (int i = 0; i + 1 < resolution; ++i) {
                uint32_t a = j * resolution + i, b = a + 1, c = a + resolution, d = c + 1;
                // Counter-clockwise seen from above (+y).
                m.indices.insert(m.indices.end(), {a, c, b, b, c, d});
            }
        }
        return m;
    }

    MeshData makeTree() {
        MeshData m;
        constexpr int sides = 6;
        uint32_t bottom = ring(m, sides, -0.5f, 0.35f, bark, 0.0f);
        uint32_t top = ring(m, sides, 3.0f, 0.25f, bark, 0.0f);
        wall(m, bottom, top, sides);
        cone(m, 2.0f, 3.0f, 4.0f, darkFoliage);
        cone(m, 4.2f, 2.3f, 3.4f, lightFoliage);
        cone(m, 6.2f, 1.5f, 3.4f, darkFoliage);
        return m;
    }

    MeshData makeRock(uint32_t seed) {
        // A subdivided octahedron pushed in and out by noise; normals left zero so createMeshModel computes them.
        MeshData m;
        std::vector<glm::vec3> corners = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        std::vector<glm::uvec3> faces = {{0, 2, 4}, {4, 2, 1}, {1, 2, 5}, {5, 2, 0},
                                         {4, 3, 0}, {1, 3, 4}, {5, 3, 1}, {0, 3, 5}};
        for (int level = 0; level < 3; ++level) {
            std::vector<glm::uvec3> next;
            for (glm::uvec3 f : faces) {
                auto mid = [&](uint32_t a, uint32_t b) {
                    corners.push_back(glm::normalize(corners[a] + corners[b]));
                    return uint32_t(corners.size() - 1);
                };
                uint32_t ab = mid(f.x, f.y), bc = mid(f.y, f.z), ca = mid(f.z, f.x);
                next.insert(next.end(), {{f.x, ab, ca}, {ab, f.y, bc}, {ca, bc, f.z}, {ab, bc, ca}});
            }
            faces = std::move(next);
        }
        glm::vec3 offset(float(seed % 97), float(seed % 31) * 1.7f, float(seed % 53) * 0.9f);
        for (glm::vec3 p : corners) {
            float r = 1.0f + 0.35f * glm::simplex(p * 1.3f + offset) + 0.12f * glm::simplex(p * 3.1f + offset);
            glm::vec3 q = p * r * glm::vec3(1.1f, 0.7f, 0.9f);
            m.vertices.push_back({q + glm::vec3(0.0f, 0.4f, 0.0f), glm::vec3(0.0f), rockGrey});
        }
        // Edge midpoints are duplicated where parent faces meet, so the computed normals give smooth patches with
        // creases between them: lumpy enough for a rock.
        for (glm::uvec3 f : faces) m.indices.insert(m.indices.end(), {f.x, f.y, f.z});
        return m;
    }

    MeshData makeWater() {
        MeshData m;
        const float s = worldSize * 0.5f;
        const glm::vec3 up(0.0f, 1.0f, 0.0f);
        m.vertices = {{glm::vec3(-s, waterLevel, -s), up, water}, {glm::vec3(s, waterLevel, -s), up, water},
                      {glm::vec3(s, waterLevel, s), up, water}, {glm::vec3(-s, waterLevel, s), up, water}};
        m.indices = {0, 3, 2, 0, 2, 1};      // counter-clockwise seen from above
        return m;
    }

    std::vector<Placement> placeTrees(int count, uint32_t seed) {
        return place(count, seed, [](float x, float z, float h, float slope, float r) {
            if (h < 3.0f || h > 360.0f || slope > 0.18f) return false;
            // Forests in patches: noise picks where, and they thin out higher up.
            float forest = glm::simplex(glm::vec2(x, z) / 700.0f) + 0.5f * glm::simplex(glm::vec2(x, z) / 180.0f);
            return forest > 0.15f && r < (forest - 0.15f) * 2.0f * (1.0f - h / 420.0f);
        });
    }

    std::vector<Placement> placeRocks(int count, uint32_t seed) {
        return place(count, seed, [](float, float, float h, float slope, float r) {
            return h > 2.0f && (slope > 0.2f || h > 300.0f) && r < 0.7f;
        });
    }
}
