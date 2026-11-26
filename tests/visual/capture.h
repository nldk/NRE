// Offscreen capture for the visual tests: renders one frame into the unsafe render target and reads it back.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace NreTest {
    // Pixels are display values 0..1 (what a viewer would show), RGB order whatever the swapchain format is.
    struct Image {
        uint32_t width = 0, height = 0;
        std::vector<glm::vec3> pixels;

        glm::vec3 at(uint32_t x, uint32_t y) const { return pixels[size_t(y) * width + x]; }
        // Mean over x0..x1 and y0..y1, both ends exclusive.
        glm::vec3 average(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) const;
    };

    // One frame in manual-submission mode, then a copy of the render target to the host.
    // SceneMode: call it without draws. FunctionDrawMode: pass the draw calls (use [] {} for none); they run
    // between renderFrame() and endFrame(). Needs init() and Unsafe::setRenderTarget() first.
    // Returns an empty image if something failed (the reason goes to stderr).
    Image renderAndCapture(const std::function<void()>& draws = {});

    void savePng(const Image& image, const std::string& path);
    Image loadPng(const std::string& path);

    // Share of pixels (0..1) whose largest channel difference is above tolerance. Different sizes count as 1.
    float fractionDifferent(const Image& a, const Image& b, float tolerance);

    using ScenarioFn = bool (*)();
    struct ScenarioRegistrar {
        ScenarioRegistrar(const char* name, ScenarioFn fn);
    };

    // `nre_visual_test <name>` runs one scenario, with no name it runs all. --update-golden: scenarios write their
    // goldens instead of comparing (they ask updatingGolden()). Returns the process exit code.
    int runScenarios(int argc, char** argv);
    bool updatingGolden();
}

// NRE_SCENARIO(baseline_car) { ... return true; } registers a scenario named "baseline-car".
#define NRE_SCENARIO(name) \
    static bool name(); \
    static NreTest::ScenarioRegistrar name##_registrar(#name, name); \
    static bool name()
