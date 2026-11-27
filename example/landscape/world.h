#pragma once
// Procedural geometry for the landscape demo: terrain, trees, rocks and a lake, all built in code and handed to
// NRE::createMeshModel(). Everything is coloured through one palette texture (palette.png, see makePalette.py):
// the terrain's UVs pick a colour by height and slope, the other meshes point at solid swatches.
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "renderer.h"

namespace Landscape {
    constexpr float worldSize = 4096.0f;    // metres, centred on the origin
    constexpr float waterLevel = 0.0f;
    constexpr int terrainResolution = 513;  // vertices per side of the terrain mesh (8 m apart)

    struct MeshData {
        std::vector<NRE::Vertex> vertices;
        std::vector<uint32_t> indices;
    };

    // Ground height in metres at (x, z) from the noise. The drawn terrain samples it every 8 m, so between samples
    // the two differ; use groundHeight() for anything that has to sit on (or stay above) the visible ground.
    float terrainHeight(float x, float z);
    // The height of the drawn terrain at (x, z): the same grid and triangle split as makeTerrain().
    float groundHeight(float x, float z);
    // Slope at (x, z): 0 flat, 1 vertical.
    float terrainSlope(float x, float z);

    // terrainResolution × terrainResolution vertices over the whole world.
    MeshData makeTerrain();
    MeshData makeTree();                    // a conifer about 10 m tall, base at the origin
    MeshData makeRock(uint32_t seed);       // a lumpy boulder about 2 m across, base at the origin
    MeshData makeWater();                   // one quad at waterLevel covering the world

    struct Placement {
        glm::vec3 position;
        float yaw;
        float scale;
    };
    // Spots where trees can grow (grassy, not too steep, above the water), or where rocks lie (rocky or steep).
    std::vector<Placement> placeTrees(int count, uint32_t seed);
    std::vector<Placement> placeRocks(int count, uint32_t seed);
}
