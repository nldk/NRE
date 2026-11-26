# Lighting System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Per-scene `Small`/`Landscape` lighting for NRE: HDR + tone mapping, real glTF materials, cascaded sun
shadows, procedural sky with auto exposure, sky-lit ambient (IBL) and haze, exposed to user shaders through
`nre/lighting.glsl`.

**Architecture:** Forward renderer. Each frame: write frame data → (sky/IBL refresh) → shadow pass into a depth
array → main pass into an `R16G16B16A16_SFLOAT` target (reverse-Z, infinite far) → sky → tone-map pass into the
swapchain or unsafe render target. The library owns all passes and per-frame data; user fragment shaders
`#include "nre/lighting.glsl"` and call `nreShade`. Pure maths lives in a header-only `lightingMath.h` so it's unit
tested on the CPU; GPU behaviour is tested by an offscreen visual-test runner that reads pixels back.

**Tech Stack:** C++26 (GCC 16), Vulkan 1.3 (dynamic rendering, sync2, dynamic cull mode), VMA, glm, fastgltf,
stb_image (+ stb_image_write for tests), glslc, CMake/CTest.

**Spec:** `docs/superpowers/specs/2026-11-26-lighting-system-design.md`, the source of truth for behaviour, API
names and defaults. Read it before any task.

## Global Constraints

- Vulkan 1.3 core only; no new heavyweight dependencies (no test framework, no MikkTSpace). stb (already fetched)
  may be used for `stb_image_write` in tests.
- Public API: plain functions with plain parameters and defaults, acting on the bound scene. No config/Desc structs.
- Every new or changed public function is mirrored in `include/renderer_functions.inl` (no default arguments there).
- Misuse checks (bad handles, wrong mode) only in Debug (`#ifndef NDEBUG`); runtime failures (files, GPU) in every build.
- Version **0.3.0** (`project(VulkanRenderer VERSION 0.3.0 ...)`), SONAME `librenderer.so.0.3`.
- Built-in shaders: `src/renderer/shaders/*.{vert,frag,comp}`, embedded via `glslc -mfmt=num` (extend the existing
  CMake rule to `.comp`). They `#include "nre/frame.glsl"` from `shaders/include/`.
- `example/main.cpp` belongs to the user: don't edit it. Example *shaders* are updated (Tasks 4, 5).
- Debug runs must produce no validation-layer messages (CTest fails on `Validation Error`).
- Defaults copied from the spec: `Small` = shadows 20 m, haze off; `Landscape` = shadows 500 m, haze visibility
  8000 m with height 300 m; exposure 1; sky colour {0.1, 0.3, 0.6}; up to 4 cascades of 2048².
- Performance: `Landscape` ≥ 165 fps at 2560x1600 on the RTX 4070 Laptop with the sun moving every frame
  (Release-unsafe); `Small` within 5% of the pre-change frame time in `nre_benchmark`.
- Work on a branch `lighting-system` (create it from `main` before Task 1). Commit at the end of every task.

## Review Focus

1. **Resizing:** the window or the unsafe render target is resized (or minimised to 0x0) mid-run. Expect HDR
   target, depth and projection to follow, with no validation errors. Test: Task 4 `resize-target`.
2. **Two scenes with different modes:** `bindScene` switches between a `Landscape` and a `Small` scene. Each keeps
   its own sky, shadow distance, haze and exposure. Test: Task 8 `scene-switch`.
3. **The IBL source cubemap is destroyed** while in use. Expect a fallback to the mode's default sky with no
   validation errors. Test: Task 9 `destroy-ibl-source`.
4. **The sun points straight down** (`{0,-1,0}`) or straight up. Expect shadow matrices to stay valid (no NaN
   from a degenerate `lookAt` up vector). Tests: Task 6 math test `cascadeViewProjVerticalSun` + visual `shadow-noon`.
5. **Meshes without UVs or normals** (OBJ files, glTF without `TEXCOORD_0`). The derivative-based normal mapping
   must not produce NaN/black pixels. Test: Task 5 `no-uv-mesh`.

---

## File structure

| File | Responsibility |
|---|---|
| `src/renderer/lighting/lightingMath.h` (new) | Header-only, glm only: tone curve + inverse, reverse-Z projections, cascade count/splits/fit, sun transmittance, auto exposure, haze density. |
| `src/renderer/lighting/shadows.{h,cpp}` (new) | Shadow depth-array image, per-layer views, comparison sampler, depth-only pipelines, shadow pass recording. |
| `src/renderer/lighting/sky.{h,cpp}` (new) | Procedural sky: transmittance LUT, sky cubemap generation, the sky draw pass (replaces `Scene::drawSkybox`). |
| `src/renderer/lighting/ibl.{h,cpp}` (new) | BRDF LUT, irradiance and prefiltered specular cubemaps from a source cubemap. |
| `src/renderer/post/toneMap.{h,cpp}` (new) | HDR colour target and the tone-map pass. |
| `src/renderer/pipeline/computePipeline.{h,cpp}` (new) | Internal compute pipelines (own layout, push constants). |
| `src/renderer/core/frameRecorder.{h,cpp}` (new) | One place that records a frame from a draw list: shadow pass → main pass → sky → tone map → unsafe callback. |
| `src/renderer/shaders/` | New built-ins: `fullscreen.vert`, `tonemap.frag`, `shadow.vert`, `shadowAlpha.frag`, `sky.vert`/`sky.frag` (replace `skybox.*`), `transmittanceLut.comp`, `skyCubemap.comp`, `irradiance.comp`, `prefilter.comp`, `brdfLut.comp`. |
| `shaders/include/nre/frame.glsl`, `lighting.glsl` (new) | The user-facing GLSL include (spec "Shader API"). |
| `cmake/NREShaders.cmake` | Adds `-I <repo>/shaders/include` to `nre_compile_shaders` and to the built-in shader rule. |
| `tests/` (new) | `CMakeLists.txt`, `lightingMathTest.cpp`, `visual/{capture.h,capture.cpp,visualTest.cpp}`, `visual/shaders/`, `visual/golden/`, `assets/makeTestModels.py` + generated models. |
| Existing files touched | `renderer.h`, `renderer_functions.inl`, `core/{renderer.cpp,context.h}`, `scene/{scene.h,scene.cpp,model.cpp,model.h}`, `gpu/{UBOAndSSBO.h,materials.{h,cpp},texture.{h,cpp},cubemap.{h,cpp},image.{h,cpp},descriptorSet.cpp}`, `pipeline/pipeline.{h,cpp}`, `unsafe/unsafe.cpp`, `CMakeLists.txt`, `example/shaders/*`. |

### Final `FrameData` layout (Task 5 adds all of it; later tasks fill fields)

Existing fields keep their offsets. `frame.glsl` mirrors this exactly; `static_assert` every offset.

| Offset | Field | Meaning |
|---|---|---|
| 0–127 | `viewProj`, `cameraPos`, `lightCount`+pad, `sunDirection`, `sunColor` | unchanged |
| 128 | `mat4 cascadeViewProj[4]` | world → shadow clip, per cascade |
| 384 | `vec4 cascadeSplits` | view-space distance where each cascade ends |
| 400 | `vec4 shadowParams` | x: cascade count (0 = no sun shadows), y: normal-offset scale (world units), zw: 0 |
| 416 | `vec4 sunRadiance` | rgb: light reaching objects (setSun colour × intensity × atmosphere tint), w: 1 if sun above horizon |
| 432 | `vec4 skyColor` | rgb: sky colour expanded with `inverseToneMapACES` |
| 448 | `vec4 skyParams` | x: sky source (0 colour, 1 cubemap, 2 procedural), y: specular mip count, z: night ambient level, w: 0 |
| 464 | `vec4 hazeParams` | x: density per meter (0 = off), y: 1 / haze height, zw: 0 |
| size 480 | | |

### Final set 0 / set 1 bindings

Set 0: 0 `FrameData` UBO · 1 lights SSBO · 2 `samplerCube` sky (cubemap or procedural) · 3 `sampler2DArrayShadow`
sun shadows (Task 6) · 4 `samplerCube` irradiance · 5 `samplerCube` prefiltered specular · 6 `sampler2D` BRDF LUT
(4–6 in Task 9). Set 1: 0 base colour · 1 metallic-roughness · 2 normal · 3 `GpuMaterial` UBO (Task 5).

---

### Task 1: Math header and CPU unit tests

**Files:**
- Create: `src/renderer/lighting/lightingMath.h`, `tests/CMakeLists.txt`, `tests/lightingMathTest.cpp`
- Modify: `CMakeLists.txt` (add `enable_testing()` + `add_subdirectory(tests)` inside `if(PROJECT_IS_TOP_LEVEL)`)

**Interfaces:**
- Produces (namespace `NRE::LightingMath`, all `inline`):
  - `float toneMapACES(float x)` (Narkowicz fit, same as `fragment.frag`), `glm::vec3 toneMapACES(glm::vec3)`
  - `float inverseToneMapACES(float y)`, `glm::vec3 inverseToneMapACES(glm::vec3)`: input clamped to [0, 0.999]
  - `glm::mat4 reverseZPerspective(float fovY, float aspect, float nearPlane)`: infinite far, depth 1 at near → 0 at infinity, Y flipped for Vulkan
  - `glm::mat4 reverseZOrtho(float width, float height, float depthRange)`: 2D, origin bottom-left like today, depth reversed
- Test helper: `tests/lightingMathTest.cpp` defines `#define CHECK(cond)` (prints file:line and sets a failure flag) and `CHECK_NEAR(a, b, eps)`; `main` returns 1 on any failure. CTest target `lightingMath`.

- [ ] **Step 1: Write the failing tests** in `tests/lightingMathTest.cpp`:
  ```cpp
  // toneMapRoundTrip: for y in {0, 0.05, 0.2, 0.5, 0.8, 0.95}: CHECK_NEAR(toneMapACES(inverseToneMapACES(y)), y, 1e-4)
  // inverseToneMapClamps: CHECK(std::isfinite(inverseToneMapACES(1.0f)) && inverseToneMapACES(1.0f) > 5.0f)
  // reverseZDepth: P = reverseZPerspective(radians(60), 16/9.f, 0.1f);
  //   depth(z) = (P * vec4(0,0,-z,1)).z / w; CHECK_NEAR(depth(0.1f), 1, 1e-5); CHECK(depth(1e6f) > 0 && depth(1e6f) < 1e-6);
  //   CHECK(depth(10) > depth(100)); CHECK(P[1][1] < 0)   // Vulkan Y flip
  // reverseZOrthoDepth: O = reverseZOrtho(800, 600, 1000); CHECK depth of z=+999 (toward viewer) > depth of z=-999
  ```
- [ ] **Step 2: Run and see it fail:** `cmake --build cmake-build-debug && ctest --test-dir cmake-build-debug -R lightingMath --output-on-failure`. Expected: compile error (header missing).
- [ ] **Step 3: Implement** `lightingMath.h` with the four function groups above. Inverse tone map: solve the fit's quadratic `(2.43y − 2.51)x² + (0.59y − 0.03)x + 0.14y = 0` for the positive root.
- [ ] **Step 4: Run tests:** same command. Expected: `100% tests passed`.
- [ ] **Step 5: Commit** `git add src/renderer/lighting tests CMakeLists.txt && git commit -m "Add lighting math header and CPU tests"`

### Task 2: Offscreen visual-test runner and baselines

**Files:**
- Create: `tests/visual/capture.h`, `tests/visual/capture.cpp`, `tests/visual/visualTest.cpp`, `tests/visual/shaders/flat.vert`, `tests/visual/shaders/flat.frag` (unlit `vec4(1,0,1,1)`), `tests/visual/golden/` (PNG baselines), `tests/perf-baseline.md`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces (`tests/visual/capture.h`, namespace `NreTest`):
  - `struct Image { uint32_t width, height; std::vector<glm::vec3> pixels; glm::vec3 at(uint32_t x, uint32_t y) const; glm::vec3 average(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) const; };` pixels are display values 0..1, RGB order regardless of swapchain format
  - `Image renderAndCapture(const std::function<void()>& draws = {})`: one frame in manual-submission mode (`renderFrame` [+ `draws` + `endFrame` in FunctionDrawMode], `submitFrame`, `presentFrame`), `vkDeviceWaitIdle`, copy the unsafe render target to a host buffer (own command pool from `Unsafe::getDevice/getGraphicsQueueFamily`), restore its layout
  - `void savePng(const Image&, const std::string& path)`, `Image loadPng(const std::string& path)`
  - `float fractionDifferent(const Image& a, const Image& b, float tolerance)`: share of pixels whose max channel diff > tolerance
  - `int runScenarios(int argc, char** argv)` with `NRE_SCENARIO(name) { ... }` registering `bool name()`; `nre_visual_test <name>` runs one; `--update-golden` writes goldens instead of comparing
- Every scenario: `setRenderConfig({Render_3D, <mode>, RenderErrorPolicy::Return, true, {640, 480, "nre_visual_test", false, false}})`, `init()`, `Unsafe::setRenderTarget(640, 480)`, …, `shutdown()`.
- CTest: built only `if(NRE_UNSAFE_API)`; one `add_test` per scenario name (list kept in `tests/CMakeLists.txt`), `FAIL_REGULAR_EXPRESSION "Validation Error"`, `ENVIRONMENT NRE_TEST_OUT=${CMAKE_CURRENT_BINARY_DIR}/out`, uses `example/assets` and the example shaders.

- [ ] **Step 1: Write scenarios** in `visualTest.cpp` (current renderer, so they define today's look):
  ```cpp
  // capture-clear: SceneMode, empty scene. CHECK every pixel ≈ sRGB-encode(0.1,0.3,0.6) within 2/255.
  // baseline-car: police.glb at (0,0,-3), camera at origin, example shaders. Save/compare golden "car.png".
  // baseline-skybox: skybox clear_day, no models, camera yaw 0. Golden "skybox.png".
  ```
- [ ] **Step 2: Build and run** `cmake --preset debug-unsafe && cmake --build cmake-build-debug-unsafe && ctest --test-dir cmake-build-debug-unsafe -R "capture-clear|baseline" --output-on-failure`. Expected: baselines fail (no goldens yet), capture-clear passes.
- [ ] **Step 3: Write goldens:** `cmake-build-debug-unsafe/bin/nre_visual_test baseline-car --update-golden` (same for skybox). Look at the PNGs before committing: car centred, sky correct.
- [ ] **Step 4: Run again.** Expected: all three pass.
- [ ] **Step 5: Record the performance baseline** (Release): `cmake --build cmake-build-release && cmake-build-release/bin/nre_benchmark --frames 600 static move`; paste the summary's render-ms numbers into `tests/perf-baseline.md`.
- [ ] **Step 6: Commit** `git commit -m "Add offscreen visual tests and pre-lighting baselines"`

### Task 3: Reverse-Z with an infinite far plane

**Files:**
- Modify: `src/renderer/core/renderer.cpp` (`updateProjection`, `beginRendering` depth clear → 0), `src/renderer/pipeline/pipeline.{h,cpp}` (default `depthCompare` → `VK_COMPARE_OP_GREATER`), skybox pipeline creation (`GREATER_OR_EQUAL`), `src/renderer/shaders/skybox.vert` (z = 0), `include/renderer_unsafe.h` (doc: depth is reverse-Z), `tests/visual/visualTest.cpp`

**Interfaces:**
- Consumes: `reverseZPerspective`, `reverseZOrtho` (Task 1).
- Produces: all scene depth is reverse-Z: cleared to 0, `GREATER` passes nearer. Later pipelines follow this.

- [ ] **Step 1: Add scenario `far-object`:** Box.glb scaled ×50 at (0, 0, −5000). CHECK the centre pixel differs from the clear colour by > 10/255 (today it's clipped at 1000 m).
- [ ] **Step 2: Run:** `ctest ... -R far-object`. Expected: FAIL.
- [ ] **Step 3: Implement:** `updateProjection` uses `reverseZPerspective(radians(60), aspect, 0.1f)` in 3D and `reverseZOrtho(width, height, 1000)` in 2D; depth clear 0; compare ops as listed. The skybox ray matrix still uses `P[0][0]`, `P[1][1]` (unchanged meaning).
- [ ] **Step 4: Run all visual tests.** Expected: `far-object`, `capture-clear`, `baseline-car`, `baseline-skybox` pass (goldens within tolerance 3/255 on ≥ 99.5% of pixels).
- [ ] **Step 5: Commit** `git commit -m "Switch to reverse-Z with an infinite far plane"`

### Task 4: HDR target, tone-map pass, exposure and sky colour

**Files:**
- Create: `src/renderer/post/toneMap.{h,cpp}`, `src/renderer/shaders/fullscreen.vert`, `src/renderer/shaders/tonemap.frag`, `src/renderer/core/frameRecorder.{h,cpp}`
- Modify: `core/renderer.cpp` (frame flow moves into `frameRecorder`), `core/context.h`, `pipeline/pipeline.cpp` (colour format = `hdrFormat`), `scene/scene.{h,cpp}` (exposure, skyColor fields), `unsafe/unsafe.cpp` (target resize also resizes the HDR target), `shaders/skybox.frag` (expand LDR with the inverse tone curve), `include/renderer.h`, `include/renderer_functions.inl`, `example/shaders/fragment.frag` (remove `toneMapACES`; output linear), `tests/visual/visualTest.cpp`

**Interfaces:**
- Produces:
  - `constexpr VkFormat hdrFormat = VK_FORMAT_R16G16B16A16_SFLOAT;` (`context.h`); the HDR image + view live in `ToneMapPass`, sized to `sceneExtent(c)`.
  - `class ToneMapPass { Result<void> create(); Result<void> resize(VkExtent2D); void record(VkCommandBuffer, float exposure, bool passthrough2D); VkImageView hdrView() const; VkImage hdrImage() const; }`
  - `Result<void> recordFrame(Context* c, Scene* scene, std::span<const QueuedDraw> draws)` in `frameRecorder.h`, where `struct QueuedDraw { Handel<Model> model; glm::mat4 matrix; bool castsShadows; };`. Records main pass (HDR) → sky → tone map → (render target: window pass) → unsafe callback.
  - Public: `Result<void> setExposure(float exposure);` `Result<void> setSkyColor(glm::vec3 color);`. The HDR clear colour = `inverseToneMapACES(skyColor) / exposure`, so the background looks like `skyColor` at any exposure.
- `FunctionDrawMode`: `drawModel`/`drawInstance` now append a `QueuedDraw` to `c->vulkan.drawQueue`; `renderFrame` only acquires and writes frame data; `endFrame` calls `recordFrame`. `SceneMode` builds the list from the scene's visible instances.

- [ ] **Step 1: Add scenarios:**
  ```cpp
  // exposure-brighter: car scene, capture A; setExposure(2); capture B; CHECK(B.average(car box) > A.average(car box) * 1.2)
  // sky-color: setSkyColor({0.8,0.2,0.1}); CHECK every pixel ≈ sRGB-encode(0.8,0.2,0.1) within 2/255
  // function-mode-car: same as baseline-car but FunctionDrawMode with drawInstance; compare golden car.png
  // resize-target: render, setRenderTarget(320,240), render, setRenderTarget(800,600), render; CHECK each capture's size matches
  // passthrough-2d: Render_2D, flat.frag shader (magenta) on a Box; CHECK its pixel == (1,0,1) within 2/255 (2D isn't tone mapped)
  ```
- [ ] **Step 2: Run:** expected FAIL (functions missing).
- [ ] **Step 3: Implement** as in Interfaces. `tonemap.frag`: `toneMapACES(hdr * exposure)` (or copy when `passthrough2D`). In 2D mode nothing else changes.
- [ ] **Step 4: Run all visual tests.** Expected: new ones pass; `capture-clear`, `baseline-car`, `baseline-skybox` still match goldens (the tone curve now runs once, in the pass).
- [ ] **Step 5: Perf check (Release):** `nre_benchmark --frames 600 static move`. Expected: render ms within 5% of `tests/perf-baseline.md`; if not, stop and report.
- [ ] **Step 6: Commit** `git commit -m "Render to an HDR target and tone map once at the end"`

### Task 5: Materials, `FrameData` layout and the GLSL include

**Files:**
- Create: `shaders/include/nre/frame.glsl`, `shaders/include/nre/lighting.glsl`, `tests/assets/makeTestModels.py` + its outputs `tests/assets/{alphaQuad.gltf,materialSpheres.gltf,noUv.obj}` (+ PNGs)
- Modify: `gpu/UBOAndSSBO.h` (full layout above), `gpu/materials.{h,cpp}`, `gpu/texture.{h,cpp}`, `gpu/descriptorSet.cpp` (pool sizes), `core/renderer.cpp` (set 1 layout, default textures, `createTexture`), `scene/model.cpp` (glTF materials), `scene/model.h` (`Model::draw` sets cull mode per mesh), `pipeline/pipeline.cpp` (`VK_DYNAMIC_STATE_CULL_MODE`), `cmake/NREShaders.cmake`, `CMakeLists.txt` (`-I` for built-ins), `include/renderer.h`, `include/renderer_functions.inl`, `example/shaders/{vertex.vert,fragment.frag}`

**Interfaces:**
- Produces:
  - `struct alignas(16) GpuMaterial { glm::vec4 baseColorFactor; float metallic, roughness, alphaCutoff, normalScale; uint32_t flags; uint32_t pad[3]; };` flags bit 0 = alpha mask, bit 1 = double-sided; 48 bytes. One host-visible UBO per `Material`.
  - `Material` gains `metallic = 1, roughness = 1, alphaCutoff = 0.5, normalScale = 1, alphaMask = false, doubleSided = false, Buffer ubo`, and `createDescriptorSet` writes set 1 bindings 0–3, falling back to defaults: white base, white MR, flat normal `(128,128,255)`.
  - Public change: `Result<Handel<Texture>> createTexture(std::string path, bool srgb = true);` (false for data maps: normal, metallic-roughness). glTF MR and normal images load with `srgb = false`.
  - `NREShaders.cmake` sets `NRE_SHADER_INCLUDE_DIR` and passes `-I` to glslc.
  - `frame.glsl`: `nreFrame` UBO (exact layout above), lights SSBO, sky `samplerCube`. `lighting.glsl`: set 1, `Surface`, `nreMaterialSurface`, `nreBRDF`, `nreLights`, `nreSunToLight`, `nreSunRadiance` (returns `sunColor.rgb * sunColor.w` until Task 8), `nreSunLight` (no shadow yet), `nreAmbient` (hemisphere from `skyColor` until Task 9), `nreShade`, `nreApplyHaze` (identity until Task 10), `nreSunShadow` (returns 1 until Task 6), `nreCascadeDebugColor` (white until Task 7). Signatures exactly as in the spec's Shader API.
  - Normal mapping: derivative-based cotangent frame from `dFdx/dFdy` of position and UV; if the UV derivative determinant is ~0 (no UVs), use the vertex normal. Back faces of double-sided materials flip the normal (`gl_FrontFacing`).
- `fragment.frag` becomes the spec's 6-line shader (`nreMaterialSurface` + `nreShade`).

- [ ] **Step 1: Generate test models:** `python3 tests/assets/makeTestModels.py` writes `alphaQuad.gltf` (2 m double-sided quad, base texture with alpha 0 in a 4×4 checker, `alphaMode: MASK`, cutoff 0.5), `materialSpheres.gltf` (two spheres: roughness 0.1 and 0.9, metallic 0, white), `noUv.obj` (cube, no `vt`, no `vn`).
- [ ] **Step 2: Add scenarios:**
  ```cpp
  // alpha-mask: alphaQuad facing camera over sky colour; CHECK a hole texel's pixel ≈ sky colour and a solid texel's pixel isn't
  // double-sided: view alphaQuad from behind; CHECK a solid texel's pixel != sky colour
  // roughness: materialSpheres lit by setSun; CHECK max luminance of the smooth sphere > rough sphere's * 1.5
  // no-uv-mesh: noUv.obj lit by setSun; CHECK no pixel inside its box is NaN-black (all > 2/255) and all finite
  // demo-car-lit: police.glb with setSun; save golden car-lit.png after visual inspection (new look: real factors)
  ```
- [ ] **Step 3: Run:** expected FAIL.
- [ ] **Step 4: Implement** per Interfaces. The car's look is now expected to change (real material factors): `baseline-car` and `function-mode-car` compare against the new golden `car-lit.png` from here on. Keep `car.png` on disk (Task 9 still reads its average). `capture-clear` and `baseline-skybox` keep their goldens.
- [ ] **Step 5: Run all tests.** Expected: pass, no validation errors.
- [ ] **Step 6: Commit** `git commit -m "Use glTF materials and add the nre/lighting.glsl include"`

### Task 6: Sun shadows (one map) for both API modes

**Files:**
- Create: `src/renderer/lighting/shadows.{h,cpp}`, `src/renderer/shaders/shadow.vert`, `src/renderer/shaders/shadowAlpha.frag`
- Modify: `lightingMath.h`, `core/frameRecorder.cpp`, `core/renderer.cpp` (set 0 binding 3), `scene/scene.{h,cpp}`, `scene/model.{h,cpp}` (`drawDepthOnly`), `include/renderer.h`, `include/renderer_functions.inl`, `shaders/include/nre/{frame,lighting}.glsl`, tests

**Interfaces:**
- Consumes: `QueuedDraw`, `recordFrame` (Task 4); `FrameData` fields (Task 5).
- Produces:
  - `LightingMath`: `uint32_t cascadeCountFor(float shadowDistance)` (≤ 30 → 1, ≤ 100 → 2, ≤ 300 → 3, else 4; 0 → 0); `std::array<glm::vec3, 8> frustumSliceCorners(const glm::mat4& view, float fovY, float aspect, float sliceNear, float sliceFar)`; `glm::mat4 cascadeViewProj(const std::array<glm::vec3, 8>& corners, glm::vec3 sunDirection, uint32_t resolution)`: bounding-sphere fit, standard-Z ortho, up vector `{1,0,0}` when |sunDirection.y| > 0.99, centre snapped to whole texels in light space.
  - `class ShadowMaps { Result<void> create(uint32_t resolution = 2048, uint32_t maxCascades = 4); void record(VkCommandBuffer, Scene&, std::span<const QueuedDraw>, uint32_t cascadeCount); VkImageView arrayView() const; VkSampler sampler() const; }`. Depth `D32_SFLOAT` array; comparison sampler (`LESS_OR_EQUAL`, linear, clamp-to-border opaque white); depth bias constant 1.25, slope 1.75; `CULL_MODE_NONE`; alpha-masked meshes use the `shadowAlpha.frag` variant.
  - Public: `Result<void> setShadowDistance(float meters);` (negative → Debug error; stored per scene; default 20); `Result<void> setCastsShadows(Handel<Instance> instance, bool castsShadows);` (default true; `drawModel` always casts).
  - Include: `nreSunShadow(worldPos, normal)` = normal offset by `shadowParams.y`, 3×3 PCF on `sampler2DArrayShadow`; `nreSunLight` multiplies by it.
- This task uses one cascade covering `[near, shadowDistance]` for every mode; Task 7 adds more.

- [ ] **Step 1: Math tests:** `cascadeCountFor(20) == 1`, `(500) == 4`, `(0) == 0`; `cascadeViewProjVerticalSun`: sun `{0,-1,0}` → all matrix entries finite, and a corner point projects inside [−1,1]².
- [ ] **Step 2: Visual scenarios** (ground = Box.glb scaled to 20×0.1×20 at y = −0.05; caster = Box at (0, 0.5, 0); sun `{-0.4,-1,-0.3}`):
  ```cpp
  // shadow-basic (SceneMode): CHECK luminance at the shadowed ground spot < 0.6 × a lit ground spot
  // shadow-function-mode: same scene through drawInstance and through drawModel; same CHECK each
  // shadow-off: setShadowDistance(0) → shadowed spot within 3% of lit spot
  // shadow-no-cast: setCastsShadows(caster, false) → same as shadow-off
  // shadow-alpha: alphaQuad as caster → the shadow has lit holes (a hole's ground spot > 0.8 × lit)
  // shadow-noon: sun {0,-1,0}; CHECK the spot directly under the caster is shadowed, no NaN
  ```
- [ ] **Step 3: Run:** expected FAIL. **Step 4: Implement.** **Step 5: Run all tests:** expected PASS.
- [ ] **Step 6: Commit** `git commit -m "Add sun shadow maps"`

### Task 7: Cascades, stabilisation and `setSceneLighting`

**Files:**
- Modify: `lightingMath.h`, `lighting/shadows.cpp`, `scene/scene.{h,cpp}`, `include/renderer.h`, `include/renderer_functions.inl`, `shaders/include/nre/lighting.glsl`, tests

**Interfaces:**
- Produces:
  - `LightingMath::std::array<float, 4> cascadeSplits(float nearPlane, float shadowDistance, uint32_t count, float lambda = 0.75f)`: practical split scheme; unused entries = `shadowDistance`.
  - Public: `enum class SceneLighting { Small, Landscape };` `Result<void> setSceneLighting(SceneLighting);` `Result<SceneLighting> getSceneLighting();`. Sets shadow distance (20 / 500) now; haze and sky defaults are wired in Tasks 8–10 (keep one `applyLightingDefaults(Scene&)` function that later tasks extend).
  - Include: cascade chosen by view distance against `cascadeSplits`, blended over the last 10% of each cascade; `nreCascadeDebugColor` tints cascades red/green/blue/yellow.

- [ ] **Step 1: Math tests:** `cascadeSplitsIncreasing` (strictly increasing, last == shadowDistance); `cascadeStable`: for two camera positions 0.3 texels apart, the projected light-space origin of both matrices differs by a whole number of texels (|frac| < 1e-3).
- [ ] **Step 2: Scenarios:**
  ```cpp
  // shadow-far: Landscape, caster box ×10 at 300 m on a 2 km ground → its shadow spot < 0.7 × lit
  // shadow-stable: Landscape, capture, move camera 1 cm sideways, capture; CHECK fractionDifferent(a, b, 8/255) < 0.01
  // scene-lighting-defaults: setSceneLighting(Landscape) then Small → shadow distance 500 then 20 (via a cascade-debug render: 4 colours visible, then 1)
  ```
- [ ] **Step 3: Run (FAIL)** → **Step 4: Implement** → **Step 5: Run all (PASS)**.
- [ ] **Step 6: Commit** `git commit -m "Add cascaded, stabilised sun shadows and scene lighting modes"`

### Task 8: Compute pipelines, procedural sky and auto exposure

**Files:**
- Create: `pipeline/computePipeline.{h,cpp}`, `lighting/sky.{h,cpp}`, `shaders/transmittanceLut.comp`, `shaders/skyCubemap.comp`, `shaders/sky.vert`, `shaders/sky.frag`
- Delete: `shaders/skybox.vert`, `shaders/skybox.frag`, `Scene::drawSkybox` (folded into `Sky`)
- Modify: `lightingMath.h`, `gpu/cubemap.{h,cpp}` (storage-image cubemaps, optional mips), `gpu/image.{h,cpp}` (`STORAGE` usage, mip views), `core/frameRecorder.cpp`, `scene/scene.cpp`, `shaders/include/nre/lighting.glsl`, tests

**Interfaces:**
- Produces:
  - `class ComputePipeline { Result<void> create(std::span<const uint32_t> spirv, std::span<const VkDescriptorSetLayoutBinding> bindings, uint32_t pushConstantSize); void dispatch(VkCommandBuffer, VkDescriptorSet, const void* push, uint32_t x, uint32_t y, uint32_t z); }`
  - `LightingMath::glm::vec3 sunTransmittance(float sunElevation)`: Rayleigh + Mie + ozone optical depth from 1.5 m above an Earth-sized planet; same constants as `transmittanceLut.comp`. `float autoExposure(float sunElevation)`: 1 for elevation ≥ 30°, rises smoothly toward the horizon and below, capped at the night level (−10° and lower).
  - `class Sky { Result<void> create(); void update(VkCommandBuffer, glm::vec3 sunDirection); void draw(VkCommandBuffer, Scene&); VkImageView cubemapView() const; }`: transmittance LUT 256×64 computed once at init; sky cubemap 256² regenerated when the sun moved > 0.25° since the last update; sun disk (0.53°) and twilight drawn in `sky.frag`, not baked; night floor ambient = `skyParams.z`.
  - `FrameData` filled: `sunRadiance` (Landscape: `sunColor × intensity × sunTransmittance(elevation)`; Small: `sunColor × intensity`), `skyParams.x` source, `skyParams.z` night level. The procedural sky's brightness scales with `intensity / 3` (spec: intensity scales sun and sky together; 3 = standard look). The tone-map exposure = `setExposure × autoExposure(elevation)` in Landscape.
  - `applyLightingDefaults`: Landscape → sky source procedural unless a skybox is set; Small → cubemap if set, else colour.

- [ ] **Step 1: Math tests:** `transmittanceZenithBrighter`: `sunTransmittance(90°)` > `sunTransmittance(2°)` per channel; `sunsetRed`: at 2°, r/b > 3; `autoExposureShape`: == 1 at 30°, 60°, 90°; non-increasing in elevation; finite and equal at −10° and −45°.
- [ ] **Step 2: Scenarios (Landscape, no models, camera pitch 0):**
  ```cpp
  // sky-noon: sun elevation 60°; CHECK top-centre pixel b > r and b > 0.4
  // sky-sunset: sun elevation 2° toward −Z, camera facing −Z; CHECK horizon pixel near the sun r > b
  // sky-night: elevation −20°; CHECK frame average luminance > 3/255 and < 0.15, b ≥ r
  // sun-disk: camera looking straight at the sun at 30°; CHECK centre pixel luminance > 0.9
  // scene-switch: scene A Landscape (sun 60°), scene B Small with sky colour red; alternate bindScene for 4 frames; CHECK each capture matches its own scene's look
  // skybox-landscape: Landscape + setSkybox(clear_day) → matches baseline-skybox golden region
  ```
- [ ] **Step 3: Run (FAIL)** → **Step 4: Implement** → **Step 5: Run all (PASS)**.
- [ ] **Step 6: Commit** `git commit -m "Add procedural sky, compute pipelines and auto exposure"`

### Task 9: Sky-lit ambient and reflections (IBL)

**Files:**
- Create: `lighting/ibl.{h,cpp}`, `shaders/brdfLut.comp`, `shaders/irradiance.comp`, `shaders/prefilter.comp`
- Modify: `core/renderer.cpp` (set 0 bindings 4–6 + defaults), `scene/scene.cpp`, `lighting/sky.cpp`, `core/frameRecorder.cpp`, `shaders/include/nre/lighting.glsl`, tests

**Interfaces:**
- Produces: `class Ibl { Result<void> create(); void filter(VkCommandBuffer, VkImageView sourceCubemap, bool sourceIsLdr); VkImageView irradianceView() const; VkImageView specularView() const; VkImageView brdfLutView() const; uint32_t specularMips() const; }`: BRDF LUT 256² once at init; irradiance 32²; specular 128² with 5 mips (GGX importance sampling). LDR sources expanded with the inverse tone curve. Cubemap skies filter once per `setSkybox`; the procedural sky refilters in the same frame `Sky::update` regenerated. One `Ibl` per scene (each scene has its own sky).
  - Include: `nreAmbient` = irradiance × diffuse + prefiltered specular × (F0 · A + B) from the LUT; with sky source 0 (colour) it keeps the hemisphere from `skyColor`.

- [ ] **Step 1: Scenarios** (white Box on ground):
  ```cpp
  // ambient-sky-blue: Landscape noon; CHECK the box's shadowed side has b > r
  // ambient-sunset-warm: Small + setSkybox(sunset); CHECK the box's lit side r > b
  // ambient-small-default: example-style scene (car, no sun, no skybox); CHECK car average ≥ 0.9 × the car region average of golden car.png (Task 2)
  // destroy-ibl-source: Small + setSkybox(cube); destroyCubemap(cube); render 3 frames; CHECK the background ≈ sky colour, no validation errors
  ```
- [ ] **Step 2: Run (FAIL)** → **Step 3: Implement** → **Step 4: Run all (PASS)**.
- [ ] **Step 5: Perf check (Release-unsafe):** add scenario `perf-landscape`: `setRenderTarget(2560, 1600)`, Landscape, demo car + ground, sun rotating 0.5°/frame, 600 frames after 60 warm-up; prints mean frame ms. Expected ≤ 6.06 ms (165 fps). If over: amortise (sky + IBL refresh at most every other frame, one face per frame), re-measure, and report the numbers.
- [ ] **Step 6: Commit** `git commit -m "Add image-based ambient and reflections from the sky"`

### Task 10: Distance haze

**Files:**
- Modify: `lightingMath.h`, `scene/scene.{h,cpp}`, `lighting/sky.cpp` + `shaders/sky.frag` (horizon haze), `include/renderer.h`, `include/renderer_functions.inl`, `shaders/include/nre/lighting.glsl`, tests

**Interfaces:**
- Produces: `LightingMath::float hazeDensityForVisibility(float visibility)` = `ln(20) / visibility`, 0 for 0. Public `Result<void> setHaze(float visibility, float height = 300.0f);` fills `hazeParams`. Include `nreApplyHaze`: height-exponential fog integrated along the view ray, colour = sky irradiance toward the view direction plus a Henyey-Greenstein forward lobe (g = 0.75) toward the sun × `sunRadiance`; `nreShade` applies it. `applyLightingDefaults`: Landscape → `setHaze(8000, 300)`, Small → off.

- [ ] **Step 1: Math test:** `hazeVisibility`: `exp(-hazeDensityForVisibility(8000) * 8000)` ≈ 0.05 (±1e-4); `hazeDensityForVisibility(0) == 0`.
- [ ] **Step 2: Scenarios:**
  ```cpp
  // haze-far: Landscape noon, box ×20 at 6 km and a box at 50 m; CHECK far box pixel within 0.08 of the horizon sky pixel beside it, near box not
  // haze-off: setHaze(0) → far box pixel differs from the sky by > 0.15
  // haze-height: box on a 600 m tall pillar at 4 km → its top is less hazed than its base
  ```
- [ ] **Step 3: Run (FAIL)** → **Step 4: Implement** → **Step 5: Run all (PASS)**.
- [ ] **Step 6: Commit** `git commit -m "Add distance haze"`

### Task 11: Finish: API docs, version, example shaders, final checks

**Files:**
- Modify: `include/renderer.h` (doc comments exactly as in the spec's Public API block, plus `createTexture`'s `srgb`), `include/renderer_functions.inl` (cross-check every new function), `CMakeLists.txt` (`VERSION 0.3.0`), `example/shaders/fragment.frag` (final 6-line form), `include/renderer_unsafe.h` (reverse-Z and HDR notes for frame-callback users), `docs/superpowers/specs/2026-11-26-lighting-system-design.md` (status: implemented)

- [ ] **Step 1: Dynamic-loading check:** build `example` once against `NRE::renderer_dynamic` in a scratch CMake project (or temporarily) and call every new function. Expected: links and runs.
- [ ] **Step 2: Full test run** in Debug-unsafe: `ctest --test-dir cmake-build-debug-unsafe --output-on-failure`. Expected: all pass, no validation errors.
- [ ] **Step 3: Release perf:** `nre_benchmark --frames 600 static move` within 5% of `tests/perf-baseline.md`; `nre_visual_test perf-landscape` ≤ 6.06 ms.
- [ ] **Step 4: Builds:** `cmake --build` for debug, debug-unsafe, release, release-unsafe all succeed without new warnings in `src/renderer`.
- [ ] **Step 5: Commit** `git commit -m "Lighting system: docs, example shaders, v0.3.0"`
