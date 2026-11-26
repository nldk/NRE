#pragma once
#include <span>

#include <glm/glm.hpp>

#include "renderer.h"

namespace NRE {
    struct Context;

    // One model drawn once this frame. FunctionDrawMode queues one per drawModel()/drawInstance() call;
    // SceneMode makes one per visible instance of the scene.
    // Fill entries in place (emplace_back(), then assign the fields): building one as a braced temporary and
    // copying it in stalls on store forwarding, which cost ~20% in the 5000-instance benchmark.
    struct QueuedDraw {
        Handel<Model> model;   // checked when recorded: a model destroyed since draws nothing
        glm::mat4 matrix;
        bool castsShadows;
    };

    // Records the whole frame into this frame slot's command buffer (begin to end; doesn't submit). Needs the
    // fence waited on, a swapchain image acquired, scene->writeFrameData() done, and c->vulkan.framePipeline set.
    //   main pass   into the HDR target (cleared to the scene's sky colour) + the scene's depth: the bound pipeline
    //               and set 0, then each draw, then the skybox
    //   tone map    HDR -> the window, or the unsafe render target (both swapchain format)
    //   (unsafe)    with a render target: the target goes to SHADER_READ_ONLY and a pass on the window begins
    //   callback    the unsafe frame callback, inside that window pass
    // scene may be null (nothing bound / destroyed): the frame is then just the default sky colour.
    Result<void> recordFrame(Context* c, Scene* scene, std::span<const QueuedDraw> draws);
}
