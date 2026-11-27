// Landscape demo: a 4 × 4 km world built in code (createMeshModel) under the Landscape lighting mode:
// procedural sky, cascaded sun shadows, sky-lit ambient, haze and a moving sun.
//
// Controls:
//   right mouse + move   look around          W A S D   fly        Space / C   up / down
//   Shift                fly fast (×8)        T / G     time of day forward / back
//   H                    haze on / off        L         Landscape / Small lighting
//   Escape               quit

#include "renderer.h"
#include "world.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <numbers>
#include <span>

using namespace NRE;

namespace {
    // Free-fly camera, kept out of the ground. Lives in the app: the library only provides the camera primitives.
    struct FlyCamera {
        glm::vec3 position{-600.0f, 0.0f, 1400.0f};
        float yaw = 0.8f;
        float pitch = 0.05f;

        void update(float dt) {
            const bool looking = isMouseButtonDown(MouseButton::Right).value();
            (void)setCursorMode(looking ? CursorMode::Locked : CursorMode::Normal);
            if (looking) {
                glm::vec2 d = getMouseDelta().value();
                yaw += d.x * 0.0025f;
                pitch = std::clamp(pitch - d.y * 0.0025f, -1.5f, 1.5f);
            }
            (void)setCameraRotation(yaw, pitch);

            glm::vec3 move(0.0f);
            if (isKeyDown(Key::W).value()) move.z += 1.0f;
            if (isKeyDown(Key::S).value()) move.z -= 1.0f;
            if (isKeyDown(Key::D).value()) move.x += 1.0f;
            if (isKeyDown(Key::A).value()) move.x -= 1.0f;
            if (isKeyDown(Key::Space).value()) move.y += 1.0f;
            if (isKeyDown(Key::C).value()) move.y -= 1.0f;
            float speed = isKeyDown(Key::LeftShift).value() ? 400.0f : 50.0f;
            if (glm::length(move) > 0.0f) {
                (void)setCameraPosition(position);
                (void)moveCameraRelative(glm::normalize(move) * speed * dt);
                position = getCameraPosition().value();
            }
            // Never below the ground or the water, never off the edge of the world.
            const float edge = Landscape::worldSize * 0.5f - 10.0f;
            position.x = std::clamp(position.x, -edge, edge);
            position.z = std::clamp(position.z, -edge, edge);
            float floor = std::max(Landscape::terrainHeight(position.x, position.z), Landscape::waterLevel) + 2.0f;
            position.y = std::max(position.y, floor);
            (void)setCameraPosition(position);
        }
    };

    // Time of day in hours → the way the sunlight travels. Sunrise at 6:00 in the east, sunset at 18:00 in the
    // west, at most 60° high at noon (and as low at midnight, below the horizon).
    glm::vec3 sunDirection(float hours) {
        const float pi = std::numbers::pi_v<float>;
        float angle = (hours - 6.0f) / 24.0f * 2.0f * pi;          // 0 at sunrise, pi at sunset
        glm::vec3 toSun(std::cos(angle), std::sin(angle) * std::sin(pi / 3.0f), -0.35f);
        return -glm::normalize(toSun);
    }

    Handel<Model> meshModel(const Landscape::MeshData& mesh, Handel<Material> material) {
        return createMeshModel(mesh.vertices, mesh.indices, material).value();
    }

    void addInstances(Handel<Model> model, std::span<const Landscape::Placement> placements) {
        for (const Landscape::Placement& p : placements) {
            Handel<Instance> instance = createInstance(model).value();
            Transform t;
            t.position = p.position;
            t.rotation = glm::angleAxis(p.yaw, glm::vec3(0.0f, 1.0f, 0.0f));
            t.scale = glm::vec3(p.scale);
            (void)setInstanceTransform(instance, t);
            (void)addInstanceToScene(instance);
        }
    }
}

int main() {
    if (!setRenderConfig({RenderMode::Render_3D, ApiMode::SceneMode, RenderErrorPolicy::Panic, false,
                          {1600, 900, "NRE landscape", true}})) return 1;
    if (!init()) return 1;

    auto vertex = createShader(SHADER_DIR "/vertex.vert.spv", ShaderType::VERTEX_SHADER).value();
    auto fragment = createShader(SHADER_DIR "/fragment.frag.spv", ShaderType::FRAGMENT_SHADER).value();
    (void)bindPipeline(createPipeline(vertex, fragment).value());
    (void)createScene();

    // One material for the whole world: every colour comes from the palette (see world.h).
    auto palette = createMaterial(createTexture(LANDSCAPE_DIR "/palette.png").value()).value();

    std::puts("Building the world...");
    (void)addInstanceToScene(createInstance(meshModel(Landscape::makeTerrain(513), palette)).value());
    (void)addInstanceToScene(createInstance(meshModel(Landscape::makeWater(), palette)).value());
    auto trees = Landscape::placeTrees(5000, 7);
    addInstances(meshModel(Landscape::makeTree(), palette), trees);
    auto rocks = Landscape::placeRocks(240, 11);
    for (int variant = 0; variant < 4; ++variant) {
        std::span<const Landscape::Placement> share(rocks.data() + rocks.size() * variant / 4,
                                                    rocks.size() / 4);
        addInstances(meshModel(Landscape::makeRock(variant * 13 + 1), palette), share);
    }
    std::printf("%zu trees, %zu rocks. Right mouse to look, WASD to fly, T/G for the time of day.\n",
                trees.size(), rocks.size());

    (void)setSceneLighting(SceneLighting::Landscape);
    float hours = 16.5f;
    (void)setSun(sunDirection(hours));
    bool haze = true, landscape = true;

    FlyCamera camera;
    float secondsSincePrint = 0.0f;
    while (!windowShouldClose().value()) {
        (void)pollEvents();
        if (isKeyPressed(Key::Escape).value()) break;
        float dt = getDeltaTime().value();

        camera.update(dt);
        float hoursPerSecond = isKeyDown(Key::LeftShift).value() ? 6.0f : 1.0f;
        if (isKeyDown(Key::T).value()) hours += hoursPerSecond * dt;
        if (isKeyDown(Key::G).value()) hours -= hoursPerSecond * dt;
        hours = std::fmod(hours + 24.0f, 24.0f);
        (void)setSunDirection(sunDirection(hours));

        if (isKeyPressed(Key::H).value()) {
            haze = !haze;
            (void)setHaze(haze ? 8000.0f : 0.0f);
        }
        if (isKeyPressed(Key::L).value()) {
            landscape = !landscape;
            (void)setSceneLighting(landscape ? SceneLighting::Landscape : SceneLighting::Small);
            if (landscape && !haze) (void)setHaze(0.0f);      // setSceneLighting resets haze to the mode's default
            std::printf("%s lighting\n", landscape ? "Landscape" : "Small");
        }
        if (isKeyPressed(Key::T).value() || isKeyPressed(Key::G).value()) {
            std::printf("%02d:%02d\n", int(hours), int(std::fmod(hours, 1.0f) * 60.0f));
        }

        (void)renderFrame();
        // NRE_LANDSCAPE_SECONDS=N: quit after N seconds (for timing runs); prints the frame rate every 2 s.
        secondsSincePrint += dt;
        if (secondsSincePrint >= 2.0f) {
            secondsSincePrint = 0.0f;
            float fps = getFPS().value();
            std::printf("%.0f fps (%.2f ms)\n", fps, 1000.0f / fps);
        }
        static const char* limit = std::getenv("NRE_LANDSCAPE_SECONDS");
        static float elapsed = 0.0f;
        elapsed += dt;
        if (limit && elapsed > std::atof(limit)) break;
    }
    (void)shutdown();
    return 0;
}
