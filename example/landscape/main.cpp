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

            // W/S along the view, A/D sideways, Space/C straight up/down (world Y, whatever the pitch).
            const glm::vec3 forward = getCameraForward().value();
            const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
            glm::vec3 move(0.0f);
            if (isKeyDown(Key::W).value()) move += forward;
            if (isKeyDown(Key::S).value()) move -= forward;
            if (isKeyDown(Key::D).value()) move += right;
            if (isKeyDown(Key::A).value()) move -= right;
            if (isKeyDown(Key::Space).value()) move.y += 1.0f;
            if (isKeyDown(Key::C).value()) move.y -= 1.0f;
            float speed = isKeyDown(Key::LeftShift).value() ? 400.0f : 50.0f;
            if (glm::length(move) > 0.0f) position += glm::normalize(move) * speed * dt;
            // Never below the ground or the water, never off the edge of the world.
            const float edge = Landscape::worldSize * 0.5f - 10.0f;
            position.x = std::clamp(position.x, -edge, edge);
            position.z = std::clamp(position.z, -edge, edge);
            float floor = std::max(Landscape::groundHeight(position.x, position.z), Landscape::waterLevel) + 2.0f;
            position.y = std::max(position.y, floor);
            (void)setCameraPosition(position);
        }
    };

    // Time of day in hours → the way the sunlight travels. Sunrise at 6:00 in the east (+x), sunset at 18:00 in the
    // west, about 68° high at noon in the south (+z, behind the start view, so the mountains are lit from the front),
    // as far below the horizon at midnight.
    glm::vec3 sunDirection(float hours) {
        const float pi = std::numbers::pi_v<float>;
        float angle = (hours - 6.0f) / 24.0f * 2.0f * pi;          // 0 at sunrise, pi at sunset
        glm::vec3 toSun(std::cos(angle), std::sin(angle) * std::sin(pi / 3.0f), 0.35f);
        return -glm::normalize(toSun);
    }

    Handel<Model> meshModel(const Landscape::MeshData& mesh, Handel<Material> material) {
        return createMeshModel(mesh.vertices, mesh.indices, material).value();
    }

    void addInstances(Handel<Model> model, std::span<const Landscape::Placement> placements, bool castsShadows = true) {
        for (const Landscape::Placement& p : placements) {
            Handel<Instance> instance = createInstance(model).value();
            Transform t;
            t.position = p.position;
            t.rotation = glm::angleAxis(p.yaw, glm::vec3(0.0f, 1.0f, 0.0f));
            t.scale = glm::vec3(p.scale);
            (void)setInstanceTransform(instance, t);
            (void)setCastsShadows(instance, castsShadows);
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
    const Landscape::Placement origin{glm::vec3(0.0f), 0.0f, 1.0f};
    addInstances(meshModel(Landscape::makeTerrain(), palette), {&origin, 1});
    addInstances(meshModel(Landscape::makeWater(), palette), {&origin, 1}, false);   // flat water casts nothing
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
    // NRE_LANDSCAPE_SECONDS=N quits after N seconds, for timing runs. The frame rate is printed every 2 s.
    const char* limitText = std::getenv("NRE_LANDSCAPE_SECONDS");
    const float limit = limitText ? float(std::atof(limitText)) : 0.0f;
    float secondsSincePrint = 0.0f, elapsed = 0.0f;
    bool firstFrame = true;     // its delta time includes building the world
    while (!windowShouldClose().value()) {
        (void)pollEvents();
        if (isKeyPressed(Key::Escape).value()) break;
        float dt = firstFrame ? 0.0f : getDeltaTime().value();
        firstFrame = false;

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
            (void)setHaze(haze ? 8000.0f : 0.0f);       // setSceneLighting resets haze to the mode's default
            std::printf("%s lighting\n", landscape ? "Landscape" : "Small");
        }
        if (isKeyPressed(Key::T).value() || isKeyPressed(Key::G).value()) {
            std::printf("%02d:%02d\n", int(hours), int(std::fmod(hours, 1.0f) * 60.0f));
        }

        (void)renderFrame();
        secondsSincePrint += dt;
        if (secondsSincePrint >= 2.0f) {
            secondsSincePrint = 0.0f;
            float fps = getFPS().value();
            std::printf("%.0f fps (%.2f ms)\n", fps, 1000.0f / fps);
        }
        elapsed += dt;
        if (limitText && elapsed > limit) break;
    }
    (void)shutdown();
    return 0;
}
