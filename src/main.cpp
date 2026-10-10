
#include "renderer.h"


int main() {
    using namespace NRE;
    setRenderConfig({RenderMode::Render_3D,RenderErrorPolicy::Panic,false,{500,500,"test",true}});
    init();
    auto model = createModel("assets/Box.glb").value();
    auto vertex = createShader(SHADER_DIR "/vertex.vert.spv",ShaderType::VERTEX_SHADER).value();
    auto frag = createShader(SHADER_DIR "/fragment.frag.spv",ShaderType::FRAGMENT_SHADER).value();
    auto pipeline = createPipeline(vertex,frag).value();
    bindPipeline(pipeline);
    while (!windowShouldClose().value()) {
        pollEvents();
        renderFrame();
    }
    shutdown();
    return 0;
}
