// SceneMode vs FunctionDrawMode under different workloads: static, moving, growing, shrinking, churning
// (adding + removing every frame), visibility toggles, mixed models, and all of it at once.
//
// Adding and removing objects is measured two ways:
//   grow / shrink / churn                 really create and destroy instances (createInstance/destroyInstance)
//   grow-pool / shrink-pool / churn-pool  instances stay alive and only stop being drawn: SceneMode takes them
//                                         out of the scene (removeInstanceFromScene/addInstanceToScene), the
//                                         function modes just skip their draw call
//
// Each scenario runs three ways:
//   SceneMode       objects are instances in the bound scene; renderFrame() draws them
//   drawInstance()  FunctionDrawMode, objects are instances (not in a scene), drawn one call each
//   drawModel()     FunctionDrawMode, no instances: the app keeps the transforms and calls drawModel()
//
// Every frame is split in two timings:
//   update  the scenario's object operations (create/destroy/transform/visibility)
//   render  renderFrame() through endFrame(): recording, submit and present (not pollEvents())
//
// Prints a summary and writes every frame to a CSV; benchmark/plot.py turns that into graphs.
// Measure a Release build: Debug adds validation layers and per-call misuse checks.
//
// Usage: nre_benchmark [--instances N] [--frames F] [--csv PATH] [scenario ...]

#include "renderer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace {
    using namespace NRE;
    using Clock = std::chrono::steady_clock;

    constexpr int warmupFrames = 60;

    template <class T>
    T check(Result<T> result, const char* what) {
        if (!result) {
            std::cerr << what << " failed: " << result.error() << '\n';
            std::exit(1);
        }
        if constexpr (!std::is_void_v<T>) return std::move(*result);
    }

    double msSince(Clock::time_point start) {
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    }

    enum class Variant { Scene, DrawInstance, DrawModel };
    constexpr Variant allVariants[] = {Variant::Scene, Variant::DrawInstance, Variant::DrawModel};

    const char* name(Variant v) {
        switch (v) {
            case Variant::Scene:        return "SceneMode";
            case Variant::DrawInstance: return "drawInstance";
            case Variant::DrawModel:    return "drawModel";
        }
        return "?";
    }

    enum class Scenario { Static, Move, Grow, GrowPool, Shrink, ShrinkPool, Churn, ChurnPool, Visibility, Mixed, Game };

    struct ScenarioInfo {
        Scenario scenario;
        std::string_view name;
        std::string_view description;
    };
    constexpr ScenarioInfo allScenarios[] = {
        {Scenario::Static,     "static",      "N boxes, nothing changes"},
        {Scenario::Move,       "move",        "every object gets a new transform every frame"},
        {Scenario::Grow,       "grow",        "starts empty, creates objects every frame until there are N"},
        {Scenario::GrowPool,   "grow-pool",   "N objects exist but aren't drawn; more get drawn every frame until all N are"},
        {Scenario::Shrink,     "shrink",      "starts with N, destroys random objects every frame until none are left"},
        {Scenario::ShrinkPool, "shrink-pool", "starts with N drawn, stops drawing random ones every frame (they stay alive)"},
        {Scenario::Churn,      "churn",       "N objects; 1% destroyed and 1% created every frame"},
        {Scenario::ChurnPool,  "churn-pool",  "N objects drawn; 1% stop being drawn and 1% from the pool start every frame"},
        {Scenario::Visibility, "visibility",  "N objects; 1% toggle visibility every frame"},
        {Scenario::Mixed,      "mixed",       "N objects spread over 3 models (2 glTF, 1 OBJ), nothing changes"},
        {Scenario::Game,       "game",        "mixed models, everything moving, plus churn and visibility toggles"},
    };

    // The benchmark's objects, kept the way an app using each variant would keep them.
    class World {
    public:
        World(Variant variant, std::vector<Handel<Model>> models, int targetCount)
            : variant(variant), models(std::move(models)),
              side(std::max(1, static_cast<int>(std::ceil(std::sqrt(targetCount))))) {}

        // Objects being drawn.
        std::size_t size() const { return objects.size(); }

        // Creates an object and draws it from now on.
        void spawn() {
            Object o = makeObject();
            if (variant == Variant::Scene) check(addInstanceToScene(o.instance), "addInstanceToScene");
            objects.push_back(o);
        }

        // Destroys a random object. destroyInstance() also takes it out of the scene.
        void despawnRandom() {
            if (objects.empty()) return;
            std::size_t i = randomIndex();
            if (variant != Variant::DrawModel) check(destroyInstance(objects[i].instance), "destroyInstance");
            objects[i] = objects.back();
            objects.pop_back();
        }

        // Creates an object in the pool: alive, but not drawn.
        void addToPool() { pool.push_back(makeObject()); }

        // Starts drawing an object from the pool.
        void activate() {
            if (pool.empty()) return;
            Object o = pool.back();
            pool.pop_back();
            if (variant == Variant::Scene) check(addInstanceToScene(o.instance), "addInstanceToScene");
            objects.push_back(o);
        }

        // Stops drawing a random object and puts it in the pool. Its instance stays alive.
        void deactivateRandom() {
            if (objects.empty()) return;
            std::size_t i = randomIndex();
            if (variant == Variant::Scene) check(removeInstanceFromScene(objects[i].instance), "removeInstanceFromScene");
            pool.push_back(objects[i]);
            objects[i] = objects.back();
            objects.pop_back();
        }

        void toggleRandom() {
            if (objects.empty()) return;
            Object& o = objects[randomIndex()];
            o.visible = !o.visible;
            if (variant != Variant::DrawModel) check(setVisibility(o.instance, o.visible), "setVisibility");
        }

        // Bob up and down and spin.
        void move(int frame) {
            for (Object& o : objects) {
                o.transform.position = o.home + glm::vec3(0.0f, 0.5f * std::sin(frame * 0.05f + o.home.x * 0.3f), 0.0f);
                o.transform.rotation = glm::angleAxis(frame * 0.02f, glm::vec3(0.0f, 1.0f, 0.0f));
                if (variant != Variant::DrawModel) check(setInstanceTransform(o.instance, o.transform), "setInstanceTransform");
            }
        }

        void draw() {
            check(renderFrame(), "renderFrame");
            if (variant == Variant::Scene) return;   // renderFrame() drew the scene
            for (const Object& o : objects) {
                if (variant == Variant::DrawInstance) {
                    check(drawInstance(o.instance), "drawInstance");   // skips hidden instances itself
                } else if (o.visible) {
                    check(drawModel(o.model, o.transform), "drawModel");
                }
            }
            check(endFrame(), "endFrame");
        }

        float extent() const { return side * spacing; }

    private:
        struct Object {
            Handel<Model> model;
            Handel<Instance> instance;   // unused by drawModel
            glm::vec3 home{0.0f};
            Transform transform;
            bool visible = true;
        };

        // A random model at a random spot on the grid. Not in the scene yet.
        Object makeObject() {
            std::uniform_int_distribution<int> cell(0, side - 1);
            std::uniform_int_distribution<std::size_t> pick(0, models.size() - 1);
            Object o;
            o.model = models[pick(rng)];
            o.home = {(cell(rng) - (side - 1) / 2.0f) * spacing, 0.0f, (cell(rng) - (side - 1) / 2.0f) * spacing};
            o.transform.position = o.home;
            if (variant != Variant::DrawModel) {
                o.instance = check(createInstance(o.model), "createInstance");
                check(setInstanceTransform(o.instance, o.transform), "setInstanceTransform");
            }
            return o;
        }

        std::size_t randomIndex() { return std::uniform_int_distribution<std::size_t>(0, objects.size() - 1)(rng); }

        static constexpr float spacing = 1.5f;
        Variant variant;
        std::vector<Handel<Model>> models;
        int side;
        std::vector<Object> objects;   // drawn
        std::vector<Object> pool;      // alive, not drawn (the -pool scenarios)
        std::mt19937 rng{1234};   // same seed for every variant, so they all do the same operations
    };

    struct FrameSample {
        std::size_t instances;
        double updateMs, renderMs;
    };

    std::vector<FrameSample> run(Scenario scenario, Variant variant, int count, int frames) {
        const ApiMode mode = variant == Variant::Scene ? ApiMode::SceneMode : ApiMode::FunctionDrawMode;
        check(setRenderConfig({RenderMode::Render_3D, mode, RenderErrorPolicy::Return, false,
                               {800, 600, "NRE benchmark", false, false}}), "setRenderConfig");
        check(init(), "init");

        auto vertex = check(createShader(SHADER_DIR "/vertex.vert.spv", ShaderType::VERTEX_SHADER), "createShader");
        auto fragment = check(createShader(SHADER_DIR "/fragment.frag.spv", ShaderType::FRAGMENT_SHADER), "createShader");
        check(bindPipeline(check(createPipeline(vertex, fragment), "createPipeline")), "bindPipeline");
        if (mode == ApiMode::SceneMode) check(createScene(), "createScene");   // FunctionDrawMode makes its own
        check(addLightToScene(check(createDirectionalLight({-0.5f, -1.0f, -0.3f}), "createDirectionalLight")), "addLightToScene");

        std::vector<Handel<Model>> models{check(createModel(ASSET_DIR "/Box.glb"), "createModel")};
        if (scenario == Scenario::Mixed || scenario == Scenario::Game) {
            models.push_back(check(createModel(ASSET_DIR "/BoxTextured.glb"), "createModel"));
            models.push_back(check(createModel(ASSET_DIR "/cube.obj"), "createModel"));
        }

        const int perFrame = std::max(1, count / 100);
        World world(variant, models, count);
        if (scenario == Scenario::GrowPool) {
            for (int i = 0; i < count; ++i) world.addToPool();
        } else if (scenario != Scenario::Grow) {
            for (int i = 0; i < count; ++i) world.spawn();
        }
        if (scenario == Scenario::ChurnPool) {
            for (int i = 0; i < perFrame; ++i) world.addToPool();   // what the first frame activates
        }
        check(setCameraPosition({0.0f, world.extent() * 0.7f, world.extent() * 0.9f + 2.0f}), "setCameraPosition");
        check(cameraLookAt({0.0f, 0.0f, 0.0f}), "cameraLookAt");

        for (int frame = 0; frame < warmupFrames; ++frame) {
            check(pollEvents(), "pollEvents");
            world.draw();
        }

        std::vector<FrameSample> samples;
        samples.reserve(frames);
        for (int frame = 0; frame < frames; ++frame) {
            check(pollEvents(), "pollEvents");
            if (check(windowShouldClose(), "windowShouldClose")) break;

            auto updateStart = Clock::now();
            switch (scenario) {
                case Scenario::Static:
                case Scenario::Mixed:
                    break;
                case Scenario::Move:
                    world.move(frame);
                    break;
                case Scenario::Grow:
                    while (world.size() < static_cast<std::size_t>(count) * (frame + 1) / frames) world.spawn();
                    break;
                case Scenario::GrowPool:
                    while (world.size() < static_cast<std::size_t>(count) * (frame + 1) / frames) world.activate();
                    break;
                case Scenario::Shrink:
                    while (world.size() > count - static_cast<std::size_t>(count) * (frame + 1) / frames) world.despawnRandom();
                    break;
                case Scenario::ShrinkPool:
                    while (world.size() > count - static_cast<std::size_t>(count) * (frame + 1) / frames) world.deactivateRandom();
                    break;
                case Scenario::Churn:
                    for (int i = 0; i < perFrame; ++i) world.despawnRandom();
                    for (int i = 0; i < perFrame; ++i) world.spawn();
                    break;
                case Scenario::ChurnPool:
                    for (int i = 0; i < perFrame; ++i) world.activate();
                    for (int i = 0; i < perFrame; ++i) world.deactivateRandom();
                    break;
                case Scenario::Visibility:
                    for (int i = 0; i < perFrame; ++i) world.toggleRandom();
                    break;
                case Scenario::Game:
                    world.move(frame);
                    for (int i = 0; i < perFrame; ++i) world.despawnRandom();
                    for (int i = 0; i < perFrame; ++i) world.spawn();
                    for (int i = 0; i < perFrame; ++i) world.toggleRandom();
                    break;
            }
            double updateMs = msSince(updateStart);

            auto renderStart = Clock::now();
            world.draw();
            samples.push_back({world.size(), updateMs, msSince(renderStart)});
        }
        check(shutdown(), "shutdown");
        return samples;
    }

    double percentile(std::vector<double> values, double p) {
        if (values.empty()) return 0.0;
        std::ranges::sort(values);
        return values[std::min(values.size() - 1, static_cast<std::size_t>(values.size() * p))];
    }

    void usage() {
        std::println("Usage: nre_benchmark [--instances N] [--frames F] [--csv PATH] [scenario ...]");
        std::println("  --instances N  objects per scenario (default 5000)");
        std::println("  --frames F     measured frames per run (default 600), after {} warm-up frames", warmupFrames);
        std::println("  --csv PATH     per-frame results (default nre_benchmark.csv); plot with benchmark/plot.py");
        std::println("Scenarios (default: all):");
        for (const ScenarioInfo& s : allScenarios) std::println("  {:<12} {}", s.name, s.description);
    }
}

int main(int argc, char** argv) {
    int count = 5000;
    int frames = 600;
    std::filesystem::path csvPath = "nre_benchmark.csv";
    std::vector<ScenarioInfo> scenarios;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        bool hasValue = i + 1 < argc;
        if (arg == "--instances" && hasValue) {
            count = std::max(1, std::atoi(argv[++i]));
        } else if (arg == "--frames" && hasValue) {
            frames = std::max(1, std::atoi(argv[++i]));
        } else if (arg == "--csv" && hasValue) {
            csvPath = argv[++i];
        } else if (auto s = std::ranges::find(allScenarios, arg, &ScenarioInfo::name); s != std::end(allScenarios)) {
            scenarios.push_back(*s);
        } else {
            usage();
            return arg == "--help" || arg == "-h" ? 0 : 1;
        }
    }
    if (scenarios.empty()) scenarios.assign(std::begin(allScenarios), std::end(allScenarios));

#ifdef NDEBUG
    constexpr const char* build = "Release";
#else
    constexpr const char* build = "Debug";
    std::println("Debug build: validation layers and per-call checks are on, so these numbers aren't representative. "
                 "Build Release to compare the modes.\n");
#endif

    std::ofstream csv(csvPath);
    if (!csv) {
        std::println(stderr, "Can't write {}", csvPath.string());
        return 1;
    }
    std::println(csv, "# build={} instances={} frames={}", build, count, frames);
    std::println(csv, "scenario,variant,frame,instances,update_ms,render_ms,total_ms");

    std::println("{} objects, {} measured frames per run after {} warm-up frames. Times are medians in ms; "
                 "p99 is of the total.\n", count, frames, warmupFrames);

    for (const ScenarioInfo& s : scenarios) {
        std::println("{} - {}", s.name, s.description);
        std::println("  {:<13} {:>9} {:>8} {:>8} {:>8} {:>8} {:>9}", "variant", "instances", "update", "render", "total", "p99", "vs scene");
        double sceneTotal = 0.0;
        for (Variant v : allVariants) {
            std::vector<FrameSample> samples = run(s.scenario, v, count, frames);

            std::vector<double> update, render, total;
            for (std::size_t f = 0; f < samples.size(); ++f) {
                const FrameSample& fs = samples[f];
                update.push_back(fs.updateMs);
                render.push_back(fs.renderMs);
                total.push_back(fs.updateMs + fs.renderMs);
                std::println(csv, "{},{},{},{},{:.5f},{:.5f},{:.5f}", s.name, name(v), f, fs.instances,
                             fs.updateMs, fs.renderMs, fs.updateMs + fs.renderMs);
            }
            double medianTotal = percentile(total, 0.5);
            if (v == Variant::Scene) sceneTotal = medianTotal;
            std::string relative = v == Variant::Scene || sceneTotal == 0.0
                ? std::string("")
                : std::format("{:+.1f}%", (medianTotal / sceneTotal - 1.0) * 100.0);
            std::println("  {:<13} {:>9} {:>8.3f} {:>8.3f} {:>8.3f} {:>8.3f} {:>9}", name(v),
                         samples.empty() ? 0 : samples.back().instances, percentile(update, 0.5),
                         percentile(render, 0.5), medianTotal, percentile(total, 0.99), relative);
            std::cout.flush();
            csv.flush();
        }
        std::println("");
    }
    std::println("Per-frame results: {}", std::filesystem::absolute(csvPath).string());
    std::println("Graphs: python3 benchmark/plot.py {}", csvPath.string());
    return 0;
}
