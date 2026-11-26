// Offscreen visual tests: each scenario renders one frame into the render target and checks the pixels.
// Run through ctest (one test per scenario) or directly: nre_visual_test <scenario> [--update-golden].
#include "capture.h"

#include <algorithm>
#include <array>
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
        explicit Session(ApiMode mode, RenderMode renderMode = RenderMode::Render_3D) {
            TRY(setRenderConfig({renderMode, mode, RenderErrorPolicy::Return, true,
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

    bool near(glm::vec3 a, glm::vec3 b, float tolerance) {
        return glm::all(glm::lessThanEqual(glm::abs(a - b), glm::vec3(tolerance)));
    }

    float brightness(glm::vec3 c) { return c.x + c.y + c.z; }

    // The police car 3 units ahead of a camera at the origin. SceneMode adds it to the scene; FunctionDrawMode
    // gets the instance back to draw itself.
    Handel<Instance> setUpCar(bool addToScene) {
        auto model = TRY(createModel(ASSET_DIR "/police.glb"));
        auto car = TRY(createInstance(model));
        if (addToScene) TRY(addInstanceToScene(car));
        TRY(translateInstance(car, glm::vec3(0.0f, 0.0f, -3.0f)));
        TRY(setCameraPosition(glm::vec3(0.0f)));
        TRY(setCameraRotation(0.0f, 0.0f));
        return car;
    }

    void setUpClearDaySkybox() {
        std::string dir = ASSET_DIR "/skyboxes/clear_day/";
        auto sky = TRY(createCubemap(dir + "right.png", dir + "left.png", dir + "top.png",
                                     dir + "bottom.png", dir + "front.png", dir + "back.png"));
        TRY(setSkybox(sky));
        TRY(setCameraPosition(glm::vec3(0.0f)));
        TRY(setCameraRotation(0.0f, 0.0f));
    }

    // The demo's sun, from above and slightly in front of a camera looking down -Z.
    void setUpSun() {
        TRY(setSun(glm::vec3(-0.4f, -1.0f, -0.3f), glm::vec3(1.0f), 3.0f));
    }

    // A model at position, seen from a camera at the origin looking down -Z. 3 m away, 1 m is about 139 px.
    Handel<Instance> placeModel(const std::string& path, glm::vec3 position) {
        auto model = TRY(createModel(path));
        auto instance = TRY(createInstance(model));
        TRY(addInstanceToScene(instance));
        TRY(translateInstance(instance, position));
        TRY(setCameraPosition(glm::vec3(0.0f)));
        TRY(setCameraRotation(0.0f, 0.0f));
        return instance;
    }

    float luminance(glm::vec3 c) { return 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z; }

    // Largest luminance over x0..x1, y0..y1 (ends exclusive).
    float maxLuminance(const Image& image, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) {
        float best = 0.0f;
        for (uint32_t y = y0; y < y1; ++y)
            for (uint32_t x = x0; x < x1; ++x) best = std::max(best, luminance(image.at(x, y)));
        return best;
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

// The police car 3 units ahead of a camera at the origin, lit by the demo shaders and the sun.
// car.png (the look before materials, without a sun) stays on disk: the ambient is compared against it later.
NRE_SCENARIO(baseline_car) {
    Session session(ApiMode::SceneMode);
    bindExamplePipeline("vertex.vert.spv", "fragment.frag.spv");
    setUpCar(true);
    setUpSun();
    return matchesGolden(renderAndCapture(), "car-lit.png");
}

// Only the sky (clear_day), camera looking down -Z. FunctionDrawMode with no draws.
NRE_SCENARIO(baseline_skybox) {
    Session session(ApiMode::FunctionDrawMode);
    bindExamplePipeline("vertex.vert.spv", "fragment.frag.spv");
    setUpClearDaySkybox();
    return matchesGolden(renderAndCapture([] {}), "skybox.png");
}

// Box.glb scaled to 50 m at 5 km: beyond the old 1000 m far plane, so it only shows with the infinite far plane.
NRE_SCENARIO(far_object) {
    Session session(ApiMode::SceneMode);
    bindExamplePipeline("vertex.vert.spv", "fragment.frag.spv");
    auto model = TRY(createModel(ASSET_DIR "/Box.glb"));
    auto box = TRY(createInstance(model));
    TRY(addInstanceToScene(box));
    TRY(scaleInstance(box, glm::vec3(50.0f)));
    TRY(translateInstance(box, glm::vec3(0.0f, 0.0f, -5000.0f)));
    TRY(setCameraPosition(glm::vec3(0.0f)));
    TRY(setCameraRotation(0.0f, 0.0f));
    Image image = renderAndCapture();
    glm::vec3 clear{srgbEncode(0.1f), srgbEncode(0.3f), srgbEncode(0.6f)};
    glm::vec3 centre = image.at(kWidth / 2, kHeight / 2);
    CHECK(glm::max(glm::max(std::abs(centre.x - clear.x), std::abs(centre.y - clear.y)), std::abs(centre.z - clear.z)) > 10.0f / 255.0f);
    return true;
}

// Exposure scales the lit car before tone mapping; the clear colour behind it stays the same.
NRE_SCENARIO(exposure_brighter) {
    Session session(ApiMode::SceneMode);
    bindExamplePipeline("vertex.vert.spv", "fragment.frag.spv");
    setUpCar(true);
    Image a = renderAndCapture();
    TRY(setExposure(2.0f));
    Image b = renderAndCapture();
    CHECK(!a.pixels.empty() && !b.pixels.empty());    // The car covers roughly x 170..470, y 45..240 (see golden/car.png).
    CHECK(brightness(b.average(190, 60, 450, 190)) > brightness(a.average(190, 60, 450, 190)) * 1.2f);
    CHECK(near(b.at(10, 400), a.at(10, 400), 2.0f / 255.0f));
#ifndef NDEBUG
    CHECK(!setExposure(0.0f));    // misuse, reported in Debug
    CHECK(!setExposure(-1.0f));
#endif
    return true;
}

// The background shows the sky colour as given, at any exposure.
NRE_SCENARIO(sky_color) {
    Session session(ApiMode::SceneMode);
    bindExamplePipeline("flat.vert.spv", "flat.frag.spv");
    TRY(setSkyColor({0.8f, 0.2f, 0.1f}));
    glm::vec3 expected{srgbEncode(0.8f), srgbEncode(0.2f), srgbEncode(0.1f)};
    for (float exposure : {1.0f, 0.25f, 4.0f}) {
        TRY(setExposure(exposure));
        Image image = renderAndCapture();
        CHECK(image.width == kWidth && image.height == kHeight);
        for (const glm::vec3& p : image.pixels) CHECK(near(p, expected, 2.0f / 255.0f));
    }
    return true;
}

// baseline-car through drawInstance: the queued draws give the same frame.
NRE_SCENARIO(function_mode_car) {
    Session session(ApiMode::FunctionDrawMode);
    bindExamplePipeline("vertex.vert.spv", "fragment.frag.spv");
    Handel<Instance> car = setUpCar(false);
    setUpSun();
    return matchesGolden(renderAndCapture([car] { TRY(drawInstance(car)); }), "car-lit.png");
}

// An LDR cubemap sky looks the same at any exposure.
NRE_SCENARIO(skybox_exposure) {
    Session session(ApiMode::FunctionDrawMode);
    bindExamplePipeline("vertex.vert.spv", "fragment.frag.spv");
    setUpClearDaySkybox();
    TRY(setExposure(2.0f));
    return matchesGolden(renderAndCapture([] {}), "skybox.png");
}

// The HDR target follows the render target's size, smaller and larger than the window.
NRE_SCENARIO(resize_target) {
    Session session(ApiMode::SceneMode);
    bindExamplePipeline("vertex.vert.spv", "fragment.frag.spv");
    setUpCar(true);
    glm::vec3 clear{srgbEncode(0.1f), srgbEncode(0.3f), srgbEncode(0.6f)};
    for (VkExtent2D size : {VkExtent2D{kWidth, kHeight}, VkExtent2D{320, 240}, VkExtent2D{800, 600}}) {
        TRY(Unsafe::setRenderTarget(size.width, size.height));
        Image image = renderAndCapture();
        CHECK(image.width == size.width && image.height == size.height);
        CHECK(near(image.at(2, size.height - 3), clear, 2.0f / 255.0f));                  // background
        CHECK(!near(image.at(size.width / 2, size.height / 4), clear, 10.0f / 255.0f));   // the car
    }
    return true;
}

// Render_2D isn't tone mapped: an unlit magenta box stays exactly magenta.
NRE_SCENARIO(passthrough_2d) {
    Session session(ApiMode::SceneMode, RenderMode::Render_2D);
    bindExamplePipeline("flat.vert.spv", "flat.frag.spv");
    auto model = TRY(createModel(ASSET_DIR "/Box.glb"));
    auto box = TRY(createInstance(model));
    TRY(addInstanceToScene(box));
    TRY(scaleInstance(box, glm::vec3(200.0f)));
    TRY(translateInstance(box, glm::vec3(kWidth / 2.0f, kHeight / 2.0f, 0.0f)));
    Image image = renderAndCapture();    CHECK(!image.pixels.empty());
    CHECK(near(image.at(kWidth / 2, kHeight / 2), glm::vec3(1.0f, 0.0f, 1.0f), 2.0f / 255.0f));
    return true;
}

// An alpha-masked quad facing the camera: the transparent cells of its checker show the sky colour.
NRE_SCENARIO(alpha_mask) {
    Session session(ApiMode::SceneMode);
    bindExamplePipeline("vertex.vert.spv", "fragment.frag.spv");
    setUpSun();
    placeModel(TEST_ASSET_DIR "/alphaQuad.gltf", glm::vec3(0.0f, 0.0f, -3.0f));
    Image image = renderAndCapture();
    CHECK(!image.pixels.empty());
    // The 2 m quad covers about x 181..459, y 101..379, in 4x4 cells of ~70 px. The top-left cell is opaque, the
    // one to its right a hole.
    glm::vec3 sky{srgbEncode(0.1f), srgbEncode(0.3f), srgbEncode(0.6f)};
    CHECK(near(image.at(285, 136), sky, 2.0f / 255.0f));
    CHECK(!near(image.at(216, 136), sky, 10.0f / 255.0f));
    return true;
}

// The double-sided quad seen from behind: its back face is drawn (not culled), lit from the camera's side.
NRE_SCENARIO(double_sided) {
    Session session(ApiMode::SceneMode);
    bindExamplePipeline("vertex.vert.spv", "fragment.frag.spv");
    TRY(setSun(glm::vec3(0.3f, -0.5f, 1.0f), glm::vec3(1.0f), 3.0f));
    placeModel(TEST_ASSET_DIR "/alphaQuad.gltf", glm::vec3(0.0f, 0.0f, -3.0f));
    TRY(setCameraPosition(glm::vec3(0.0f, 0.0f, -6.0f)));
    TRY(cameraLookAt(glm::vec3(0.0f, 0.0f, -3.0f)));
    Image image = renderAndCapture();
    CHECK(!image.pixels.empty());
    // Mirrored: the front's opaque top-left cell is now at the top right, with a hole to its left.
    glm::vec3 sky{srgbEncode(0.1f), srgbEncode(0.3f), srgbEncode(0.6f)};
    CHECK(!near(image.at(424, 136), sky, 10.0f / 255.0f));
    CHECK(near(image.at(355, 136), sky, 2.0f / 255.0f));
    return true;
}

// Two white spheres, roughness 0.1 (left) and 0.9 (right): the smooth one has a much brighter highlight.
// A dark sky and a low exposure keep the rough one well below the top of the tone curve.
NRE_SCENARIO(roughness) {
    Session session(ApiMode::SceneMode);
    bindExamplePipeline("vertex.vert.spv", "fragment.frag.spv");
    TRY(setSun(glm::vec3(-0.3f, -0.5f, -1.0f), glm::vec3(1.0f), 3.0f));
    TRY(setSkyColor(glm::vec3(0.02f)));
    TRY(setExposure(0.1f));
    placeModel(TEST_ASSET_DIR "/materialSpheres.gltf", glm::vec3(0.0f, 0.0f, -3.0f));
    Image image = renderAndCapture();
    CHECK(!image.pixels.empty());
    float smooth = maxLuminance(image, 0, 0, kWidth / 2, kHeight);
    float rough = maxLuminance(image, kWidth / 2, 0, kWidth, kHeight);
    std::printf("max luminance: smooth %.3f, rough %.3f\n", smooth, rough);
    CHECK(smooth > rough * 1.5f);
    return true;
}

// An OBJ cube without UVs or normals: the derivative-based normal must not give NaN or black pixels.
NRE_SCENARIO(no_uv_mesh) {
    Session session(ApiMode::SceneMode);
    bindExamplePipeline("vertex.vert.spv", "fragment.frag.spv");
    setUpSun();
    auto cube = placeModel(TEST_ASSET_DIR "/noUv.obj", glm::vec3(0.0f, 0.0f, -3.0f));
    TRY(rotateInstance(cube, glm::radians(35.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
    TRY(rotateInstance(cube, glm::radians(25.0f), glm::vec3(1.0f, 0.0f, 0.0f)));
    Image image = renderAndCapture();
    CHECK(!image.pixels.empty());
    // The 1 m cube is ~139 px across around the centre; this box is inside it at any rotation.
    for (uint32_t y = 200; y < 280; ++y) {
        for (uint32_t x = 280; x < 360; ++x) {
            glm::vec3 p = image.at(x, y);
            CHECK(!glm::any(glm::isnan(p)) && !glm::any(glm::isinf(p)));
            CHECK(glm::all(glm::greaterThan(p, glm::vec3(2.0f / 255.0f))));
        }
    }
    return true;
}

namespace {
    // Brightness of quad i (centre ~139 px per metre from the image centre) of a row of 0.5 m quads 0.6 m apart,
    // n of them, 3 m from the camera (see makeTestModels.py write_quad_row).
    float quadBrightness(const Image& image, int i, int n) {
        int cx = int(std::lround(kWidth / 2.0 + (i - (n - 1) / 2.0) * 0.6 * 139.0));
        return brightness(image.average(uint32_t(cx - 15), kHeight / 2 - 15, uint32_t(cx + 15), kHeight / 2 + 15));
    }
}

// Constant normal maps tilted 45 degrees (flat, +X, -X, +Y, -Y left to right). A sun from the right brightens the
// +X tilt and darkens the -X one; a sun from above does the same for +Y and -Y. A sign error on either tangent axis fails.
NRE_SCENARIO(normal_map) {
    Session session(ApiMode::SceneMode);
    bindExamplePipeline("vertex.vert.spv", "fragment.frag.spv");
    TRY(setSkyColor(glm::vec3(0.02f)));
    TRY(setExposure(0.25f));
    placeModel(TEST_ASSET_DIR "/normalQuads.gltf", glm::vec3(0.0f, 0.0f, -3.0f));

    TRY(setSun(glm::vec3(-1.0f, 0.0f, -1.0f), glm::vec3(1.0f), 3.0f));   // toward the sun: +x and +z
    Image fromRight = renderAndCapture();
    CHECK(!fromRight.pixels.empty());
    float flat = quadBrightness(fromRight, 0, 5), plusX = quadBrightness(fromRight, 1, 5), minusX = quadBrightness(fromRight, 2, 5);
    std::printf("sun from +x: flat %.3f, +X %.3f, -X %.3f\n", flat, plusX, minusX);
    CHECK(plusX > flat * 1.05f);
    CHECK(minusX < flat * 0.7f);

    TRY(setSun(glm::vec3(0.0f, -1.0f, -1.0f), glm::vec3(1.0f), 3.0f));   // toward the sun: +y and +z
    Image fromAbove = renderAndCapture();
    CHECK(!fromAbove.pixels.empty());
    flat = quadBrightness(fromAbove, 0, 5);
    float plusY = quadBrightness(fromAbove, 3, 5), minusY = quadBrightness(fromAbove, 4, 5);
    std::printf("sun from +y: flat %.3f, +Y %.3f, -Y %.3f\n", flat, plusY, minusY);
    CHECK(plusY > flat * 1.05f);
    CHECK(minusY < flat * 0.7f);
    return true;
}

// Metallic-roughness textures are read as linear data with metallic in blue: metallic 0.5 from the texture looks
// like metallic 0.5 from the factor (an sRGB decode would read 0.22, much brighter), and a fully metal texel loses
// its diffuse light (a rough white metal still reflects the sun, so it is only somewhat darker).
NRE_SCENARIO(metallic_texture) {
    Session session(ApiMode::SceneMode);
    bindExamplePipeline("vertex.vert.spv", "fragment.frag.spv");
    TRY(setSkyColor(glm::vec3(0.02f)));
    TRY(setExposure(0.5f));
    TRY(setSun(glm::vec3(-0.3f, -0.5f, -1.0f), glm::vec3(1.0f), 3.0f));
    placeModel(TEST_ASSET_DIR "/metalQuads.gltf", glm::vec3(0.0f, 0.0f, -3.0f));
    Image image = renderAndCapture();
    CHECK(!image.pixels.empty());
    float fromTexture = quadBrightness(image, 0, 3), fromFactor = quadBrightness(image, 1, 3), metal = quadBrightness(image, 2, 3);
    std::printf("half metal: texture %.3f, factor %.3f; full metal %.3f\n", fromTexture, fromFactor, metal);
    CHECK(std::abs(fromTexture - fromFactor) < 3.0f * 3.0f / 255.0f);
    CHECK(metal < fromFactor * 0.9f);
    return true;
}

// The police car with the sun, using its real material factors.
NRE_SCENARIO(demo_car_lit) {
    Session session(ApiMode::SceneMode);
    bindExamplePipeline("vertex.vert.spv", "fragment.frag.spv");
    setUpCar(true);
    setUpSun();
    return matchesGolden(renderAndCapture(), "car-lit.png");
}

namespace {
    // Where a world point lands in the 640x480 capture: the renderer's camera (60 degree vertical field of view)
    // with the bound scene's view. Vulkan's framebuffer y points down.
    glm::vec2 toPixel(glm::vec3 world) {
        glm::mat4 projection = glm::perspective(glm::radians(60.0f), float(kWidth) / float(kHeight), 0.1f, 100.0f);
        glm::vec4 clip = projection * TRY(getViewMatrix()) * glm::vec4(world, 1.0f);
        glm::vec2 ndc = glm::vec2(clip) / clip.w;
        return {(ndc.x * 0.5f + 0.5f) * float(kWidth), (0.5f - ndc.y * 0.5f) * float(kHeight)};
    }

    // Luminance of the 7x7 pixels around where a world point lands. -1 if that's not inside the image.
    float spotLuminance(const Image& image, glm::vec3 world) {
        glm::vec2 p = toPixel(world);
        if (p.x < 4.0f || p.y < 4.0f || p.x > float(kWidth) - 5.0f || p.y > float(kHeight) - 5.0f) return -1.0f;
        uint32_t x = uint32_t(p.x), y = uint32_t(p.y);
        return luminance(image.average(x - 3, y - 3, x + 4, y + 4));
    }

    // The shadow scenes: a 20 x 20 m ground (Box.glb flattened, its top at y = 0), a 1 m box standing on it at the
    // origin, the demo sun (it travels toward -x and -z, so the box's shadow falls on its -x side). The camera looks
    // at that side from the -x side.
    struct ShadowScene {
        Handel<Model> box;
        Transform ground, caster;
        Handel<Instance> groundInstance, casterInstance;
    };
    ShadowScene setUpShadowScene(bool addToScene, glm::vec3 casterPosition = {0.0f, 0.5f, 0.0f}) {
        ShadowScene s;
        s.box = TRY(createModel(ASSET_DIR "/Box.glb"));
        s.ground.position = {0.0f, -0.05f, 0.0f};
        s.ground.scale = {20.0f, 0.1f, 20.0f};
        s.caster.position = casterPosition;
        s.groundInstance = TRY(createInstance(s.box));
        TRY(setInstanceTransform(s.groundInstance, s.ground));
        s.casterInstance = TRY(createInstance(s.box));
        TRY(setInstanceTransform(s.casterInstance, s.caster));
        if (addToScene) {
            TRY(addInstanceToScene(s.groundInstance));
            TRY(addInstanceToScene(s.casterInstance));
        }
        setUpSun();
        TRY(setCameraPosition({-4.0f, 3.0f, 1.0f}));
        TRY(cameraLookAt({-0.5f, 0.0f, -0.3f}));
        return s;
    }
    // On the ground beside the box, where the box blocks the sun: the sun ray from here passes through it.
    constexpr glm::vec3 kShadowedSpot{-0.7f, 0.0f, -0.3f};
    // Open ground about as far from the camera, at a similar angle.
    constexpr glm::vec3 kLitSpot{-0.7f, 0.0f, 1.2f};

    // The box's shadow is clearly darker than the open ground.
    bool hasBoxShadow(const Image& image, const char* label) {
        CHECK(!image.pixels.empty());
        float shadowed = spotLuminance(image, kShadowedSpot), lit = spotLuminance(image, kLitSpot);
        std::printf("%s: shadowed spot %.3f, lit spot %.3f\n", label, shadowed, lit);
        CHECK(shadowed >= 0.0f && lit > 0.0f);
        CHECK(shadowed < 0.6f * lit);
        return true;
    }

    // No shadow: the spot beside the box looks like the open ground.
    bool hasNoBoxShadow(const Image& image) {
        CHECK(!image.pixels.empty());
        float shadowed = spotLuminance(image, kShadowedSpot), lit = spotLuminance(image, kLitSpot);
        std::printf("shadowed spot %.3f, lit spot %.3f\n", shadowed, lit);
        CHECK(shadowed >= 0.0f && lit > 0.0f);
        CHECK(std::abs(shadowed - lit) <= 0.03f * lit);
        return true;
    }

    void saveCapture(const Image& image, const std::string& file) {
        std::filesystem::create_directories(outDir());
        savePng(image, outDir() + "/" + file);
    }

    // Shadows must not darken what the sun reaches (mild acne, a lit side dimmed by the lookup): litSpot in image
    // is within 3% of the same spot rendered again with sun shadows off. Leaves the shadow distance at 0.
    bool litMatchesUnshadowed(const Image& image, glm::vec3 litSpot, const std::function<void()>& draws = {}) {
        CHECK(!image.pixels.empty());
        TRY(setShadowDistance(0.0f));
        Image unshadowed = renderAndCapture(draws);
        CHECK(!unshadowed.pixels.empty());
        float lit = spotLuminance(image, litSpot), reference = spotLuminance(unshadowed, litSpot);
        std::printf("lit spot %.4f, same spot without shadows %.4f\n", lit, reference);
        CHECK(lit > 0.0f && reference > 0.0f);
        CHECK(std::abs(lit - reference) <= 0.03f * reference);
        return true;
    }
}

// A box on the ground casts the sun's shadow (SceneMode, default shadow distance).
NRE_SCENARIO(shadow_basic) {
    Session session(ApiMode::SceneMode);
    bindExamplePipeline("vertex.vert.spv", "fragment.frag.spv");
    setUpShadowScene(true);
    Image image = renderAndCapture();
    saveCapture(image, "shadow-basic.png");
    if (!hasBoxShadow(image, "scene")) return false;
    return litMatchesUnshadowed(image, kLitSpot);
}

// The same scene in FunctionDrawMode: queued through drawInstance, then through drawModel.
NRE_SCENARIO(shadow_function_mode) {
    Session session(ApiMode::FunctionDrawMode);
    bindExamplePipeline("vertex.vert.spv", "fragment.frag.spv");
    ShadowScene s = setUpShadowScene(false);
    Image instances = renderAndCapture([&] {
        TRY(drawInstance(s.groundInstance));
        TRY(drawInstance(s.casterInstance));
    });
    saveCapture(instances, "shadow-function-mode-instance.png");
    if (!hasBoxShadow(instances, "drawInstance")) return false;
    Image models = renderAndCapture([&] {
        TRY(drawModel(s.box, s.ground));
        TRY(drawModel(s.box, s.caster));
    });
    saveCapture(models, "shadow-function-mode-model.png");
    return hasBoxShadow(models, "drawModel");
}

// Shadow distance 0 turns sun shadows off. Negative distances are misuse (reported in Debug).
NRE_SCENARIO(shadow_off) {
    Session session(ApiMode::SceneMode);
    bindExamplePipeline("vertex.vert.spv", "fragment.frag.spv");
    setUpShadowScene(true);
    TRY(setShadowDistance(0.0f));
    Image image = renderAndCapture();
    saveCapture(image, "shadow-off.png");
#ifndef NDEBUG
    CHECK(!setShadowDistance(-1.0f));
#endif
    return hasNoBoxShadow(image);
}

// An instance that doesn't cast shadows leaves the ground lit (it's still drawn).
NRE_SCENARIO(shadow_no_cast) {
    Session session(ApiMode::SceneMode);
    bindExamplePipeline("vertex.vert.spv", "fragment.frag.spv");
    ShadowScene s = setUpShadowScene(true);
    TRY(setCastsShadows(s.casterInstance, false));
    Image image = renderAndCapture();
    saveCapture(image, "shadow-no-cast.png");
    return hasNoBoxShadow(image);
}

// The alpha-masked checker quad held flat 2 m above the ground: its shadow has lit holes where the cut-outs are.
NRE_SCENARIO(shadow_alpha) {
    Session session(ApiMode::SceneMode);
    bindExamplePipeline("vertex.vert.spv", "fragment.frag.spv");
    auto box = TRY(createModel(ASSET_DIR "/Box.glb"));
    auto ground = TRY(createInstance(box));
    TRY(setInstanceTransform(ground, Transform{{0.0f, -0.05f, 0.0f}, glm::identity<glm::quat>(), {20.0f, 0.1f, 20.0f}}));
    TRY(addInstanceToScene(ground));
    // Lying flat: local (x, y, 0) goes to (x, 2, -y), so the quad's top row of cells (opaque, hole, ...) is at z = -0.75.
    auto quad = placeModel(TEST_ASSET_DIR "/alphaQuad.gltf", glm::vec3(0.0f, 2.0f, 0.0f));
    TRY(rotateInstance(quad, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)));
    setUpSun();
    TRY(setCameraPosition({-1.0f, 3.5f, -5.0f}));
    TRY(cameraLookAt({-1.2f, 0.0f, -1.4f}));
    Image image = renderAndCapture();
    saveCapture(image, "shadow-alpha.png");
    CHECK(!image.pixels.empty());
    // The sun moves a point at height 2 by (-0.8, -2, -0.6) on its way to the ground.
    const glm::vec3 drop{-0.8f, -2.0f, -0.6f};
    float opaque = spotLuminance(image, glm::vec3(-0.75f, 2.0f, -0.75f) + drop);   // top-left cell
    float hole = spotLuminance(image, glm::vec3(-0.25f, 2.0f, -0.75f) + drop);     // the cell to its right
    float lit = spotLuminance(image, {-1.3f, 0.0f, -2.5f});                         // outside the quad's shadow
    std::printf("opaque cell's spot %.3f, hole's spot %.3f, open ground %.3f\n", opaque, hole, lit);
    CHECK(opaque >= 0.0f && hole >= 0.0f && lit > 0.0f);
    CHECK(opaque < 0.6f * lit);
    CHECK(hole > 0.8f * lit);
    return true;
}

// The sun straight down (the light's up vector has to change): the box, lifted 1 m off the ground, shadows the
// ground right below it.
NRE_SCENARIO(shadow_noon) {
    Session session(ApiMode::SceneMode);
    bindExamplePipeline("vertex.vert.spv", "fragment.frag.spv");
    setUpShadowScene(true, {0.0f, 1.5f, 0.0f});
    TRY(setSun(glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(1.0f), 3.0f));
    TRY(setCameraPosition({-4.0f, 2.5f, 1.0f}));
    TRY(cameraLookAt({0.0f, 0.0f, 0.0f}));
    Image image = renderAndCapture();
    saveCapture(image, "shadow-noon.png");
    CHECK(!image.pixels.empty());
    float below = spotLuminance(image, {0.0f, 0.0f, 0.0f}), lit = spotLuminance(image, {-1.5f, 0.0f, 1.0f});
    std::printf("below the box %.3f, open ground %.3f\n", below, lit);
    CHECK(std::isfinite(below) && std::isfinite(lit));
    CHECK(below >= 0.0f && lit > 0.0f);
    CHECK(below < 0.6f * lit);
    return true;
}

namespace {
    // The landscape scenes: a 2 x 2 km ground (Box.glb flattened, its top at y = 0) and boxes standing on it ahead of
    // the camera (down -z) at 20, 50, 100 and 250 m, 1, 2, 4 and 10 m tall, so every cascade has a shadow in view, and
    // a sun from the side.
    // The camera stands 10 m up at the origin and looks a little down, from 14 m ahead of it to the horizon.
    void setUpLandscapeScene() {
        auto box = TRY(createModel(ASSET_DIR "/Box.glb"));
        auto ground = TRY(createInstance(box));
        TRY(setInstanceTransform(ground, Transform{{0.0f, -0.5f, 0.0f}, glm::identity<glm::quat>(), {2000.0f, 1.0f, 2000.0f}}));
        TRY(addInstanceToScene(ground));
        const struct { float distance, size; } boxes[] = {{20, 1}, {50, 2}, {100, 4}, {250, 10}};
        for (auto [distance, size] : boxes) {
            auto caster = TRY(createInstance(box));
            TRY(setInstanceTransform(caster, Transform{{0.0f, size * 0.5f, -distance}, glm::identity<glm::quat>(), glm::vec3(size)}));
            TRY(addInstanceToScene(caster));
        }
        // From the right and a little behind the camera: the shadows fall to the boxes' left, in view.
        TRY(setSun(glm::vec3(-1.0f, -0.8f, 0.4f), glm::vec3(1.0f), 3.0f));
        TRY(setCameraPosition({0.0f, 10.0f, 0.0f}));
        TRY(cameraLookAt({0.0f, 0.0f, -100.0f}));
    }

    // Which cascade tint a pixel shows (nreCascadeDebugColor): 0 red, 1 green, 2 blue, 3 yellow, -1 none.
    int cascadeTint(glm::vec3 c) {
        const float d = 0.2f;
        if (c.r > c.g + d && c.r > c.b + d) return 0;
        if (c.g > c.r + d && c.g > c.b + d) return 1;
        if (c.b > c.r + d && c.b > c.g + d) return 2;
        if (c.r > c.b + d && c.g > c.b + d && std::abs(c.r - c.g) < 0.1f) return 3;
        return -1;
    }

    // Share of the image's pixels showing each cascade tint.
    std::array<float, 4> cascadeTintShares(const Image& image) {
        std::array<float, 4> shares{};
        for (glm::vec3 c : image.pixels) {
            if (int t = cascadeTint(c); t >= 0) shares[t] += 1.0f;
        }
        for (float& s : shares) s /= float(image.pixels.size());
        std::printf("cascade tints: red %.4f, green %.4f, blue %.4f, yellow %.4f\n", shares[0], shares[1], shares[2], shares[3]);
        return shares;
    }
}

// Landscape: a 10 m box 300 m away still casts a clear shadow on a 2 km ground.
NRE_SCENARIO(shadow_far) {
    Session session(ApiMode::SceneMode);
    bindExamplePipeline("vertex.vert.spv", "fragment.frag.spv");
    TRY(setSceneLighting(SceneLighting::Landscape));
    auto box = TRY(createModel(ASSET_DIR "/Box.glb"));
    auto ground = TRY(createInstance(box));
    TRY(setInstanceTransform(ground, Transform{{0.0f, -0.5f, 0.0f}, glm::identity<glm::quat>(), {2000.0f, 1.0f, 2000.0f}}));
    TRY(addInstanceToScene(ground));
    auto caster = TRY(createInstance(box));
    TRY(setInstanceTransform(caster, Transform{{0.0f, 5.0f, -300.0f}, glm::identity<glm::quat>(), glm::vec3(10.0f)}));
    TRY(addInstanceToScene(caster));
    // A low sun travelling toward -x: the box's shadow reaches 20 m out from its -x face.
    TRY(setSun(glm::vec3(-1.0f, -0.5f, 0.0f), glm::vec3(1.0f), 3.0f));
    // From high up on the +z side, about 300 m from the box: it's in the last cascade.
    TRY(setCameraPosition({0.0f, 220.0f, -100.0f}));
    TRY(cameraLookAt({-10.0f, 0.0f, -300.0f}));
    Image image = renderAndCapture();
    saveCapture(image, "shadow-far.png");
    CHECK(!image.pixels.empty());
    const glm::vec3 shadowSpot{-15.0f, 0.0f, -300.0f}, litSpot{-15.0f, 0.0f, -270.0f};
    float shadowed = spotLuminance(image, shadowSpot), lit = spotLuminance(image, litSpot);
    std::printf("shadow spot %.3f, lit spot %.3f\n", shadowed, lit);
    CHECK(shadowed >= 0.0f && lit > 0.0f);
    CHECK(shadowed < 0.7f * lit);
    return litMatchesUnshadowed(image, litSpot);
}

// Landscape: moving the camera 1 cm sideways leaves the frame almost unchanged. That catches gross instability (e.g. a
// cascade whose size or placement jumps when the camera moves), not sub-texel swimming: with 3x3 PCF an unsnapped
// map shifted by a fraction of a texel changes too few pixels by too little to show here. Texel snapping is guarded
// by cascadeStable in tests/lightingMathTest.cpp. Also checks the near cascades are sharp.
NRE_SCENARIO(shadow_stable) {
    Session session(ApiMode::SceneMode);
    bindExamplePipeline("vertex.vert.spv", "fragment.frag.spv");
    TRY(setSceneLighting(SceneLighting::Landscape));
    setUpLandscapeScene();
    Image a = renderAndCapture();
    saveCapture(a, "shadow-stable-a.png");
    TRY(moveCameraRelative({0.01f, 0.0f, 0.0f}));
    Image b = renderAndCapture();
    saveCapture(b, "shadow-stable-b.png");
    CHECK(!a.pixels.empty() && !b.pixels.empty());
    float changed = fractionDifferent(a, b, 8.0f / 255.0f);
    std::printf("pixels changed by the 1 cm move: %.4f%%\n", changed * 100.0f);
    CHECK(changed < 0.01f);
    // The 1 m box 20 m away still casts a crisp shadow: the near cascades are sharp, not one map stretched to 500 m.
    float shadowed = spotLuminance(b, {-1.0f, 0.0f, -19.7f}), lit = spotLuminance(b, {2.0f, 0.0f, -19.7f});
    std::printf("near box: shadow spot %.3f, lit spot %.3f\n", shadowed, lit);
    CHECK(shadowed >= 0.0f && lit > 0.0f);
    CHECK(shadowed < 0.6f * lit);
    // Open ground between the near boxes, in the first cascades.
    return litMatchesUnshadowed(b, {6.0f, 0.0f, -30.0f});
}

// Landscape: a long wall's shadow runs from near the camera through every cascade border without a gap or a
// lighter band where the cascades blend.
NRE_SCENARIO(shadow_cascade_borders) {
    Session session(ApiMode::SceneMode);
    bindExamplePipeline("vertex.vert.spv", "fragment.frag.spv");
    TRY(setSceneLighting(SceneLighting::Landscape));
    auto box = TRY(createModel(ASSET_DIR "/Box.glb"));
    auto ground = TRY(createInstance(box));
    TRY(setInstanceTransform(ground, Transform{{0.0f, -0.5f, 0.0f}, glm::identity<glm::quat>(), {2000.0f, 1.0f, 2000.0f}}));
    TRY(addInstanceToScene(ground));
    // 3 m high, from 10 m to 410 m ahead. The sun from the right lays its shadow along its left side, 3.75 m wide.
    auto wall = TRY(createInstance(box));
    TRY(setInstanceTransform(wall, Transform{{0.0f, 1.5f, -210.0f}, glm::identity<glm::quat>(), {0.5f, 3.0f, 400.0f}}));
    TRY(addInstanceToScene(wall));
    TRY(setSun(glm::vec3(-1.0f, -0.8f, 0.4f), glm::vec3(1.0f), 3.0f));
    TRY(setCameraPosition({0.0f, 10.0f, 0.0f}));
    TRY(cameraLookAt({0.0f, 0.0f, -100.0f}));
    Image image = renderAndCapture();
    saveCapture(image, "shadow-cascade-borders.png");
    CHECK(!image.pixels.empty());
    // In each blend band and right after each border (the camera looks down -z, so z is about minus the view depth).
    for (float distance : {29.0f, 33.0f, 64.0f, 70.0f, 131.0f, 141.0f}) {
        float shadowed = spotLuminance(image, {-2.0f, 0.0f, -distance}), lit = spotLuminance(image, {5.0f, 0.0f, -distance});
        std::printf("%.0f m: shadow spot %.3f, lit spot %.3f\n", distance, shadowed, lit);
        CHECK(shadowed >= 0.0f && lit > 0.0f);
        CHECK(shadowed < 0.6f * lit);
    }
    return litMatchesUnshadowed(image, {5.0f, 0.0f, -64.0f});
}

// setSceneLighting picks the shadow distance: Landscape 500 m (4 cascades in view), then Small 20 m (1). The sun
// and the sky colour are kept.
NRE_SCENARIO(scene_lighting_defaults) {
    Session session(ApiMode::SceneMode);
    auto vertex = TRY(createShader(std::string(SHADER_DIR) + "/vertex.vert.spv", ShaderType::VERTEX_SHADER));
    auto fragment = TRY(createShader(std::string(SHADER_DIR) + "/cascadeDebug.frag.spv", ShaderType::FRAGMENT_SHADER));
    TRY(bindPipeline(TRY(createPipeline(vertex, fragment))));
    CHECK(TRY(getSceneLighting()) == SceneLighting::Small);
    setUpLandscapeScene();
    TRY(setSkyColor(glm::vec3(0.5f)));   // grey, so the sky doesn't count as a tint
    const glm::vec3 sun = TRY(getSunDirection());

    TRY(setSceneLighting(SceneLighting::Landscape));
    CHECK(TRY(getSceneLighting()) == SceneLighting::Landscape);
    Image landscape = renderAndCapture();
    saveCapture(landscape, "scene-lighting-defaults-landscape.png");
    CHECK(!landscape.pixels.empty());
    for (float share : cascadeTintShares(landscape)) CHECK(share > 0.002f);

    TRY(setSceneLighting(SceneLighting::Small));
    CHECK(TRY(getSceneLighting()) == SceneLighting::Small);
    Image small = renderAndCapture();
    saveCapture(small, "scene-lighting-defaults-small.png");
    CHECK(!small.pixels.empty());
    auto shares = cascadeTintShares(small);
    CHECK(shares[0] > 0.002f);
    CHECK(shares[1] == 0.0f && shares[2] == 0.0f && shares[3] == 0.0f);
    CHECK(near(TRY(getSunDirection()), sun, 1e-6f));
    CHECK(near(small.at(kWidth / 2, 2), glm::vec3(srgbEncode(0.5f)), 2.0f / 255.0f));   // the sky colour, kept
#ifndef NDEBUG
    CHECK(!setSceneLighting(SceneLighting(7)));
    CHECK(TRY(getSceneLighting()) == SceneLighting::Small);
#endif
    return true;
}

int main(int argc, char** argv) {
    return runScenarios(argc, argv);
}
