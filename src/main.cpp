#include "renderer.h"

#include <algorithm>
#include <cmath>


struct OrbitCamera {
    glm::vec3 target{0.0f};
    float distance = 3.0f;
    float yaw = 0.0f;
    float pitch = 0.0f;

    static constexpr float rotateSpeed = 0.01f;
    static constexpr float zoomStep = 0.9f;
    static constexpr float maxPitch = 1.55f;

    void update() {
        using namespace NRE;
        if (isMouseButtonDown(MouseButton::Left).value()) {
            glm::vec2 d = getMouseDelta().value();

            yaw -= d.x * rotateSpeed;
            pitch = std::clamp(pitch + d.y * rotateSpeed, -maxPitch, maxPitch);
        }

        distance = std::max(distance * std::pow(zoomStep, getScrollDelta().value()), 0.1f);

        glm::vec3 offset{std::cos(pitch) * std::sin(yaw), std::sin(pitch), std::cos(pitch) * std::cos(yaw)};
        setCameraPosition(target + offset * distance);
        cameraLookAt(target);
    }
};

int main() {
    using namespace NRE;
    setRenderConfig({RenderMode::Render_3D,RenderErrorPolicy::Ignore,false,{500,500,"test",true}});
    init();
    auto model = createModel(ASSET_DIR "/police.glb").value();
    auto car = createInstance(model).value();
    auto vertex = createShader(SHADER_DIR "/vertex.vert.spv",ShaderType::VERTEX_SHADER).value();
    auto frag = createShader(SHADER_DIR "/fragment.frag.spv",ShaderType::FRAGMENT_SHADER).value();
    auto pipeline = createPipeline(vertex,frag).value();
    bindPipeline(pipeline);
    auto scene = createScene();
    addInstanceToScene(car);
    translateInstance(car,glm::vec3(0.0f,0.0f,-1.0f));

    OrbitCamera camera;
    camera.target = getInstanceTransform(car).value().position;
    while (!windowShouldClose().value()) {
        pollEvents();
        camera.update();
        renderFrame();
        rotateInstance(car,0.0001f,glm::vec3(0.0f,1.0f,0.0f));
    }
    shutdown();
    return 0;
}
