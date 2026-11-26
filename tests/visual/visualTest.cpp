// Offscreen visual tests: each scenario renders one frame into the render target and checks the pixels.
// Run through ctest (one test per scenario) or directly: nre_visual_test <scenario> [--update-golden].
#include "capture.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>

#include "renderer.h"
#include "renderer_unsafe.h"

using namespace NRE;
using namespace NreTest;

#define CHECK(cond) \
    do { if (!(cond)) { std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); return false; } } while (0)

// Unwraps a Result; on an error prints the renderer's message and ends the process (a scenario can't go on).
#define TRY(expr) unwrap((expr), __FILE__, __LINE__)

namespace {
    template <class T>
    decltype(auto) unwrap(Result<T>&& r, const char* file, int line) {
        if (!r) {
            std::cerr << file << ":" << line << ": " << r.error() << "\n";
            std::exit(1);
        }
        if constexpr (!std::is_void_v<T>) return std::move(*r);
    }

    constexpr uint32_t kWidth = 640, kHeight = 480;
    constexpr float kGoldenTolerance = 3.0f / 255.0f;
    constexpr float kGoldenMaxFraction = 0.005f;

    // init() .. shutdown() around a scenario, with the 640x480 render target set.
    struct Session {
        explicit Session(ApiMode mode) {
            TRY(setRenderConfig({RenderMode::Render_3D, mode, RenderErrorPolicy::Return, true,
                                 {int(kWidth), int(kHeight), "nre_visual_test", false, false}}));
            TRY(init());
            TRY(Unsafe::setRenderTarget(kWidth, kHeight));
            // FunctionDrawMode already made a scene in init().
            if (mode == ApiMode::SceneMode) TRY(createScene());
        }
        ~Session() { (void)shutdown(); }
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;
    };

    float srgbEncode(float c) {
        return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
    }

    std::string outDir() {
        const char* env = std::getenv("NRE_TEST_OUT");
        return env ? env : "out";
    }

    // Writes the golden with --update-golden, otherwise compares against it (saving the capture to NRE_TEST_OUT on a miss).
    bool matchesGolden(const Image& image, const std::string& file) {
        CHECK(!image.pixels.empty());
        std::string golden = std::string(GOLDEN_DIR) + "/" + file;
        if (updatingGolden()) {
            savePng(image, golden);
            std::printf("wrote %s\n", golden.c_str());
            return true;
        }
        Image expected = loadPng(golden);
        if (expected.pixels.empty()) {
            std::fprintf(stderr, "missing golden %s (run with --update-golden)\n", golden.c_str());
            savePng(image, outDir() + "/" + file);
            return false;
        }
        float fraction = fractionDifferent(image, expected, kGoldenTolerance);
        if (fraction > kGoldenMaxFraction) {
            std::string actual = outDir() + "/" + file;
            savePng(image, actual);
            std::fprintf(stderr, "%s differs from golden on %.2f%% of pixels, capture saved to %s\n",
                         file.c_str(), fraction * 100.0f, actual.c_str());
            return false;
        }
        return true;
    }

    void bindExamplePipeline(const char* vertexFile, const char* fragmentFile) {
        auto vertex = TRY(createShader(std::string(SHADER_DIR) + "/" + vertexFile, ShaderType::VERTEX_SHADER));
        auto fragment = TRY(createShader(std::string(SHADER_DIR) + "/" + fragmentFile, ShaderType::FRAGMENT_SHADER));
        auto pipeline = TRY(createPipeline(vertex, fragment));
        TRY(bindPipeline(pipeline));
    }
}

// SceneMode, empty scene: every pixel is the clear color {0.1, 0.3, 0.6} (linear), stored as sRGB.
NRE_SCENARIO(capture_clear) {
    Session session(ApiMode::SceneMode);
    bindExamplePipeline("flat.vert.spv", "flat.frag.spv");
    Image image = renderAndCapture();
    CHECK(image.width == kWidth && image.height == kHeight);
    glm::vec3 expected{srgbEncode(0.1f), srgbEncode(0.3f), srgbEncode(0.6f)};
    for (const glm::vec3& p : image.pixels)
        CHECK(glm::all(glm::lessThanEqual(glm::abs(p - expected), glm::vec3(2.0f / 255.0f))));
    return true;
}

// The police car 3 units ahead of a camera at the origin, lit by the demo shaders.
NRE_SCENARIO(baseline_car) {
    Session session(ApiMode::SceneMode);
    bindExamplePipeline("vertex.vert.spv", "fragment.frag.spv");
    auto model = TRY(createModel(ASSET_DIR "/police.glb"));
    auto car = TRY(createInstance(model));
    TRY(addInstanceToScene(car));
    TRY(translateInstance(car, glm::vec3(0.0f, 0.0f, -3.0f)));
    TRY(setCameraPosition(glm::vec3(0.0f)));
    TRY(setCameraRotation(0.0f, 0.0f));
    return matchesGolden(renderAndCapture(), "car.png");
}

// Only the sky (clear_day), camera looking down -Z. FunctionDrawMode with no draws.
NRE_SCENARIO(baseline_skybox) {
    Session session(ApiMode::FunctionDrawMode);
    bindExamplePipeline("vertex.vert.spv", "fragment.frag.spv");
    std::string dir = ASSET_DIR "/skyboxes/clear_day/";
    auto sky = TRY(createCubemap(dir + "right.png", dir + "left.png", dir + "top.png",
                                 dir + "bottom.png", dir + "front.png", dir + "back.png"));
    TRY(setSkybox(sky));
    TRY(setCameraPosition(glm::vec3(0.0f)));
    TRY(setCameraRotation(0.0f, 0.0f));
    return matchesGolden(renderAndCapture([] {}), "skybox.png");
}

int main(int argc, char** argv) {
    return runScenarios(argc, argv);
}
