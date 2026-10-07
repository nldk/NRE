#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "renderer.h"


int main() {
    Renderer::setRenderConfig({Renderer::RenderMode::Render_3D,Renderer::RenderErrorPolicy::Panic,{500,500,"test"}});

    return 0;
}
