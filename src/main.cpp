
#include "renderer.h"


int main() {
    using namespace NRE;
    setRenderConfig({RenderMode::Render_3D,RenderErrorPolicy::Panic,false,{500,500,"test",true}});
    init();
    auto model = createModel(ASSET_DIR "/Box.glb").value();
    auto vertex = createShader(SHADER_DIR "/vertex.vert.spv",ShaderType::VERTEX_SHADER).value();
    auto frag = createShader(SHADER_DIR "/fragment.frag.spv",ShaderType::FRAGMENT_SHADER).value();
    auto pipeline = createPipeline(vertex,frag).value();
    bindPipeline(pipeline);
    auto scene = createScene();
    addModelToScene(model);
    translateModel(model,glm::vec3(0.0f,0.0f,-1.0f));
    while (!windowShouldClose().value()) {
        pollEvents();
        renderFrame();
        rotateModel(model,0.0001f,glm::vec3(0.0f,1.0f,0.0f));
    }
    shutdown();
    return 0;
}
