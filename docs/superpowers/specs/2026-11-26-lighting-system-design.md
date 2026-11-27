# Lighting system design

Date: 2026-11-26. Status: implemented (see the plan and ledger for deviations).

## Intent

The user wants to render **beautiful landscapes and nature** with NRE, with a **moving sun** (time of day: dawn,
noon, dusk all look right), and still be able to render **normal, small scenes** (a single car, a room) without the
landscape machinery getting in the way. The switch between the two is a per-scene toggle.

The user is learning Vulkan by building this renderer and writes their own shaders. The lighting system must leave
the fragment shader in their hands: the library does the heavy lifting and ships GLSL they `#include`, so their
shader shrinks to a few lines but stays theirs to read and bend.

## Where things stand (2026-11-26, commit 8b6b311)

- Forward renderer, Vulkan 1.3 dynamic rendering, one pass straight into the swapchain (or the unsafe render
  target). `beginFrame` in `src/renderer/core/renderer.cpp` starts that pass immediately.
- Set 0 (per scene, per frame in flight): binding 0 `FrameData` UBO (viewProj, cameraPos, lightCount, sunDirection,
  sunColor), binding 1 lights SSBO, binding 2 skybox `samplerCube` (black default). Set 1: material, only binding 0
  base color texture. Push constants: model + normal matrix (vertex stage).
- Sun: `setSun` / `setSunDirection` / `getSunDirection`, per scene, intensity 0 = no sun.
- Skybox: `createCubemap` (six 8-bit sRGB faces, no mips) + `setSkybox`; drawn by a built-in pipeline after the
  scene at the far plane. Built-in shaders live in `src/renderer/shaders/` and are embedded with `glslc -mfmt=num`.
- The example fragment shader (`example/shaders/fragment.frag`) does GGX + Lambert, hardcodes metallic 0 /
  roughness 0.5, uses a constant ambient, and applies ACES tone mapping itself. It ignores the sun and the skybox.
- The glTF loader reads only the base color texture and `baseColorFactor` (stored, not used). No metallic/roughness,
  normal maps, alpha mode or double-sided flag. `Vertex` is position/normal/uv (32 bytes), no tangents.
- `FunctionDrawMode`: `drawModel`/`drawInstance` record straight into the open main pass.
- Six CC0 skyboxes in `example/assets/skyboxes/`.

## Goals

1. **Per-scene lighting mode** with two presets:

   | | `Landscape` | `Small` (default, today's behaviour) |
   |---|---|---|
   | Sky | procedural, from the sun direction | the scene's cubemap, else the clear color |
   | Ambient + reflections | from the procedural sky, updated as the sun moves | from the cubemap, computed once per `setSkybox` |
   | Sun shadows | cascades, far (default 500 m) | one map, short range (default 20 m) |
   | Distance haze | on | off |

   Shadow distance, haze and the sky source are separate settings; the mode only picks their defaults. A small
   scene with haze, or a landscape with a photo cubemap sky, must be possible.
2. **HDR rendering**: the scene renders into a floating-point image; one tone-mapping pass at the end writes the
   swapchain (or the unsafe render target). Exposure is adjustable.
3. **Sun shadows**: cascaded shadow maps that are stable when the camera moves (no shimmering), soft-edged (PCF),
   without acne or detached shadows on the demo assets. Work in both `SceneMode` and `FunctionDrawMode`.
4. **Sky-lit ambient and reflections** (image-based lighting) from whichever sky is active.
5. **Procedural sky** driven by the sun direction: blue day sky, coloured sunsets/sunrises, a visible sun disk, and
   a sun colour that reddens near the horizon (the sun's light on objects follows it).
6. **Distance haze (aerial perspective)**: far terrain fades toward the sky colour, warmer toward the sun.
7. **glTF materials** used for real: base color factor, metallic/roughness (factors + texture), normal maps,
   alpha mask (foliage cut-outs, also in the shadow pass), double-sided.
8. **`nre/lighting.glsl`**: the include the user's shaders use. One-call path (`nreShade`) plus the pieces
   (shadow lookup, ambient, haze, BRDF) for custom looks.
9. The example shaders move to the include; existing point/directional lights keep working (unshadowed).

## Non-goals

- Shadows for point lights or spot lights; spot lights at all.
- Alpha-blended transparency (glass, water surfaces), refraction, SSR, SSAO, GI, bloom, auto-exposure.
  (The HDR pass is where bloom/auto-exposure would go later.)
- Volumetric clouds; clouds in the procedural sky.
- Terrain or vegetation generation/rendering systems (LOD, instancing of grass). The lighting must handle models
  that contain foliage; it doesn't produce them.
- A material editing API beyond what glTF files carry.
- Deferred shading.
- A time-of-day controller in the library: the app animates `setSunDirection`.

## Anti-goals (failure even if it technically works)

- Small scenes looking worse or noticeably slower than today.
- The user having to write lighting math to get the standard look.
- Shadows shimmering/crawling as the camera moves, or visible popping at cascade borders.
- Trees casting square shadows (alpha mask ignored in the shadow pass).
- Validation-layer errors in Debug.
- Config/"Desc" structs in the public API (see constraints).

## Constraints

- Vulkan 1.3 core only (dynamic rendering, synchronization2; dynamic cull mode is available and may be used).
- No new heavyweight dependencies. Built-in shaders embedded like the skybox ones.
- Public API style: plain functions with plain parameters and defaults, per-scene settings act on the bound scene
  (like the camera, sun and skybox). No descriptor/config structs.
- Misuse checks only in Debug; runtime failures (missing files, GPU errors) checked in every build.
- Every new public function goes into `include/renderer_functions.inl` (dynamic loading). Breaking changes bump
  the minor version (this work: **0.3.0**, SONAME `librenderer.so.0.3`).
- The app owns controllers: the library adds primitives, not behaviour.
- The user writes `example/main.cpp`; test through a scratchpad app. The example *shaders* are updated (they must
  stop tone mapping, see below).

## Approaches considered

- **A. Library computes, user's shader includes `nre/lighting.glsl`** (chosen by the user). The library owns all
  per-frame work (shadow maps, sky, IBL maps, haze parameters) and ships the GLSL; the user's fragment shader calls
  it. Keeps shader writing with the user; the include must match the library version.
- B. Fixed built-in lit shader for all models. Simplest to use, but custom looks would need a second path and it
  takes shader writing away from the user.
- C. Library provides data only, user writes the lighting math. Hands most of the work back; doesn't fit the ask.

## Decided

### Public API (C++, `include/renderer.h`)

All new functions act on the **bound scene**, like the camera, sun and skybox, and go into `renderer_functions.inl`.

```cpp
enum class SceneLighting {
    // Default; today's behaviour. Sky: the scene's cubemap, else the sky colour. One sharp shadow map over a short
    // distance, no haze, exposure is exactly setExposure.
    Small,
    // Procedural sky computed from the sun, sky-lit ambient and reflections, cascaded shadows reaching far, haze,
    // and an automatic exposure that follows the sun's height (setExposure multiplies on top).
    Landscape,
};

// Switches the bound scene's mode and resets shadow distance, haze and the sky to that mode's defaults:
//   Small:     shadows 20 m, haze off,                    sky = cubemap if set, else the sky colour
//   Landscape: shadows 500 m, haze visibility 8000 m,     sky = procedural (or the cubemap if set)
// The sun, lights, exposure and sky colour are kept.
Result<void> setSceneLighting(SceneLighting lighting);
Result<SceneLighting> getSceneLighting();

// How far from the camera the sun casts shadows, in meters. 0 turns sun shadows off.
// The number of cascades follows from it (up to 4); shorter distances give sharper shadows.
Result<void> setShadowDistance(float meters);
// Whether an instance casts shadows (it still receives them). Default true. E.g. off for grass, for speed.
// drawModel() always casts.
Result<void> setCastsShadows(Handel<Instance> instance, bool castsShadows);

// Distance haze. visibility: meters at which the haze hides almost everything (about 95%); 0 turns it off.
// height: how thick the haze layer is; it thins out above that height (meters above y = 0), so mountain tops
// poke out of valley haze.
Result<void> setHaze(float visibility, float height = 300.0f);

// Brightness multiplier applied before tone mapping. 1 = the standard look. In Landscape mode it's on top of the
// automatic exposure, so 1 looks right at any time of day.
Result<void> setExposure(float exposure);

// Small mode without a cubemap: the background colour, and the colour the ambient light is taken from
// (sky-tinted from above, darker from below). Replaces the hard-coded clear colour. Default {0.1, 0.3, 0.6}.
Result<void> setSkyColor(glm::vec3 color);

// Existing, behaviour extended:
//   setSun(direction, color, intensity): in Landscape mode the light reaching objects is color × intensity × the
//     atmosphere's tint (reddening at sunset), and intensity also scales the procedural sky's brightness.
//   setSkybox(cubemap): that cubemap is the sky (and ambient/reflection source) in either mode;
//     setSkybox({}) goes back to the mode's default sky.
//   createMaterial(base, metallicRoughness, normal): the second and third textures are now used.
```

Not added (app side or out of scope): a time-of-day controller or angle helpers (the app computes the sun direction),
material factor setters, per-light shadows.

### Shader API (`nre/lighting.glsl`, `#include`d by the user's shaders)

`nre/frame.glsl` declares set 0 (`nreFrame` UBO, lights, sky, shadow maps) and is enough for vertex shaders;
`nre/lighting.glsl` includes it and adds set 1 (material) and the functions:

```glsl
struct Surface {
    vec3  position;    // world space
    vec3  normal;      // world space, normal map applied, flipped for back faces of double-sided materials
    vec3  view;        // unit vector from the surface toward the camera
    vec3  albedo;      // linear
    float alpha;
    float roughness;   // perceptual, 0..1
    float metallic;
};

// Reads the bound material (textures × factors), applies the normal map, and discards the fragment if the
// material is alpha-masked and alpha < cutoff.
Surface nreMaterialSurface(vec3 worldPos, vec3 vertexNormal, vec2 uv);
// Everything: sun (with shadows) + scene lights + sky ambient and reflections, then haze. Linear HDR out.
vec3 nreShade(Surface s);

// The pieces nreShade is made of, for custom looks:
vec3  nreSunLight(Surface s);                     // direct sun, BRDF × shadow × atmosphere tint
float nreSunShadow(vec3 worldPos, vec3 normal);   // 0 = fully shadowed, 1 = lit (PCF, cascade blending)
vec3  nreSunToLight();                            // unit vector toward the sun
vec3  nreSunRadiance();                           // sun colour × intensity × atmosphere tint
vec3  nreLights(Surface s);                       // point + directional lights, unshadowed
vec3  nreAmbient(Surface s);                      // sky diffuse + specular reflections
vec3  nreBRDF(Surface s, vec3 toLight, vec3 radiance);   // GGX + Lambert, for your own lights
vec3  nreApplyHaze(vec3 color, vec3 worldPos);
vec3  nreCascadeDebugColor(vec3 worldPos);        // tints each cascade, for tuning
```

### HDR and tone mapping (builder's call, agreed in conversation)
- Scene color target `R16G16B16A16_SFLOAT` at scene size (window or unsafe render target), plus depth.
  User pipelines are created against that format.
- A built-in fullscreen pass tone maps (ACES fit, as today) with the scene's exposure into the swapchain or the
  unsafe render target, before the unsafe frame callback runs. The unsafe API keeps returning a swapchain-format
  render target, so ImGui code doesn't change.
- **Breaking:** user shaders output linear HDR and must not tone map. The skybox shader outputs linear HDR (8-bit
  sRGB cubemaps decode to linear on sampling).
- `Render_2D`: the final pass copies without tone mapping so 2D output looks the same as before; lighting features
  (shadows, sky, haze) are skipped.

### Frame structure
`wait fence → write frame data → [per-frame sky/IBL updates] → shadow pass(es) → main pass (geometry, then sky) →
tone-map pass → unsafe callback → present`. `FunctionDrawMode` queues `drawModel`/`drawInstance` calls as
`{model, matrix}` and replays the queue at `endFrame()` into the shadow pass and the main pass; apps see no change.

### Shadows
- Cascaded shadow maps for the sun in one depth 2D-array image (default 4 cascades × 2048² for `Landscape`,
  1 cascade for `Small`), comparison sampler with border = lit.
- Cascades stabilized: bounding-sphere fit + texel snapping in light space.
- Depth-only built-in pipeline; a second variant with a fragment stage for alpha-masked materials.
- Depth bias plus a normal offset in the shader; PCF in the include.
- Cascade matrices and split distances in `FrameData` (appended; existing fields keep their offsets).

### Materials
- Set 1 grows: base color, metallic-roughness, normal textures (+ defaults: white, white, flat normal) and the
  factors/flags (base color factor, metallic, roughness, alpha cutoff, alpha mode, double-sided).
- Normal maps use a derivative-based tangent frame in the shader (no tangents in `Vertex`, no MikkTSpace).
- Double-sided via dynamic cull mode per mesh.

### Sky, ambient and haze
- Procedural sky: physically based single scattering (Rayleigh + Mie) from the sun direction, producing sunsets;
  rendered into a small cubemap that the sky pass draws and the IBL is filtered from.
- IBL: diffuse irradiance + GGX-prefiltered specular cubemap with mips + BRDF lookup table. Cubemap mode filters once
  per `setSkybox`; procedural mode refilters when the sun has moved enough (budgeted so moving the sun every frame
  stays smooth).
- Haze: analytic height/distance fog in the include, coloured from the sky toward the view direction with a forward
  scattering lobe toward the sun. Also applied to the sky near the horizon so terrain and sky meet.
- In `Landscape` mode the sun's light colour on objects = `setSun` colour × atmospheric transmittance.

### The include
- Shipped in the repo (e.g. `shaders/include/nre/lighting.glsl`); `nre_compile_shaders` adds the include path so
  projects that fetch NRE get it too. Declares the set 0/1 bindings, `Surface`, `nreSurface(...)`,
  `nreShade(Surface)`, and the pieces (`nreSunShadow`, `nreAmbient`, `nreHaze`, BRDF terms, `nreAlphaTest`).
- Carries a version define checked against the library at pipeline creation in Debug if feasible.

### Build order (each step leaves the renderer working)
1. HDR target + tone-map pass + exposure; example shaders stop tone mapping.
2. glTF materials into set 1.
3. Shadow pass infrastructure + `FunctionDrawMode` draw queue; single sun shadow map (`Small`).
4. Cascades + stabilization (`Landscape`).
5. Procedural sky.
6. IBL (cubemap mode, then procedural refresh).
7. Haze.
8. Alpha mask / double-sided (incl. shadow pass).
9. `nre/lighting.glsl` finalized; example shaders rewritten on it; scratchpad landscape test with a moving sun.

### Brightness across the day (decided by the user)
- The atmosphere, sun and IBL are physically based internally, but in `Landscape` mode the library applies an
  **automatic exposure driven by the sun's elevation** (a smooth curve, not histogram auto-exposure), so noon,
  golden hour and dusk all read well at `setExposure(1)`. Dusk still looks like dusk: darker and warmer, not
  normalised to noon.
- `setExposure` multiplies on top of it. In `Small` mode there's no automatic part: exposure is just `setExposure`.
- **Sun below the horizon:** the sky and lighting fade smoothly through twilight to a dim blue night floor
  (a faint ambient from the night sky, no sun light, no sun shadows). No stars or moon.
- `setSun` intensity scales the sun's light on objects and the sky brightness together; the default 3 gives the
  standard look.

### Settled after the builder check (Claude's calls, open to the user's correction)
- **`Small` mode with no cubemap:** ambient falls back to a built-in sky/ground hemisphere derived from the
  clear colour (today's constant ambient, generalised), never black. `example/main.cpp` with no lights must look
  at least as good as today.
- **LDR sources (8-bit cubemaps, clear colour) under tone mapping:** they're display-referred, so they're expanded
  with the inverse of the tone curve when used as sky/background. After tone mapping at exposure 1, a cubemap sky
  and the clear colour look the same as today. The same expanded values feed IBL, which also lifts their ambient
  out of the 0..1 range. No new public API.
- **Projection:** reverse-Z with an infinite far plane, internal only (no public setter in this work). Every
  built-in and user pipeline uses `GREATER(_OR_EQUAL)`, depth clears to 0, and the sky is drawn at depth 0.
  Decided before step 3 because cascades, haze and the sky depend on it. The unsafe API's depth format stays
  `D32_SFLOAT`; anyone drawing depth in the unsafe frame callback must flip their compare op (documented).
- **Performance target:** the user's machine (RTX 4070 Laptop, 2560x1600 @ 165 Hz panel). `Landscape` should
  hold **≥165 fps at 2560x1600** on the demo assets plus a test landscape while the sun moves every frame. `Small`
  stays within ~5% of today's frame time in the existing benchmark. Compute pipelines are added (step 6) for sky
  and IBL generation.

## Left to the builder
- Exact atmosphere model and its LUTs, cubemap/LUT resolutions, cascade split scheme, PCF kernel, bias values,
  IBL refresh budget: tuned against the demo assets and a test landscape.
- Compute vs fragment passes for sky/IBL generation (compute pipelines don't exist yet).
- Descriptor layout details within the set 0 / set 1 split.
- Function names within the API shape above.
