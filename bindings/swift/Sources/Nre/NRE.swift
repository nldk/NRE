import CNre

/// The NRE renderer: every function of nre.h, see renderer.h for what each one does.
///
/// Main actor: NRE drives a GLFW window, and GLFW must be used from the main thread. The error messages are
/// also per thread (nreGetLastErrorMessage), so keeping every call on one thread keeps them right. Call it from
/// `@main`/top-level code or anything else on the main actor. Only `version` and `errorCodeToString`, which
/// touch no renderer state, are nonisolated.
///
/// Every function throws `NreError` when the C call doesn't return NRE_OK, and the first call throws
/// `.libraryLoadFailed` if the loaded libnre_c isn't a 0.3.x.
@MainActor
public enum NRE {
    // MARK: Errors and version

    /// The loaded library's version (nreGetVersion).
    public nonisolated static var version: (major: UInt32, minor: UInt32, patch: UInt32) {
        var major: UInt32 = 0, minor: UInt32 = 0, patch: UInt32 = 0
        nreGetVersion(&major, &minor, &patch)
        return (major, minor, patch)
    }

    /// Throws `.libraryLoadFailed` if the loaded libnre_c isn't the 0.3 ABI these bindings target.
    /// Every other call checks this too; call it to fail early.
    public nonisolated static func checkVersion() throws(NreError) {
        try requireCompatibleLibrary()
    }

    /// "Ok", "FileNotFound", ... (nreErrorCodeToString).
    public nonisolated static func errorCodeToString(_ code: ErrorCode) -> String {
        String(cString: nreErrorCodeToString(code.c))
    }

    /// The message of the most recent failed call on this thread ("" if none failed yet).
    /// NreError already carries it; this is for completeness.
    public static func lastErrorMessage() -> String {
        String(cString: nreGetLastErrorMessage())
    }

    // MARK: Setup and frames

    public static func setRenderConfig(width: Int, height: Int, title: String,
                                       mode: RenderMode = .render3D,
                                       apiMode: ApiMode = .sceneMode,
                                       errorPolicy: RenderErrorPolicy = .return,
                                       manualFramePresentationAndSubmission: Bool = false,
                                       resizable: Bool = true,
                                       showFpsInTitle: Bool = true) throws(NreError) {
        try call {
            nreSetRenderConfig(mode.c, apiMode.c, errorPolicy.c, manualFramePresentationAndSubmission,
                               Int32(width), Int32(height), title, resizable, showFpsInTitle)
        }
    }

    /// nreInit (`init` is reserved in Swift).
    public static func initialize() throws(NreError) { try call { nreInit() } }
    public static func shutdown() throws(NreError) { try call { nreShutdown() } }
    /// SceneMode: draws the bound scene and (unless manual presentation) submits and presents it.
    /// FunctionDrawMode: starts the frame; then drawModel/drawInstance calls, then endFrame.
    public static func renderFrame() throws(NreError) { try call { nreRenderFrame() } }
    /// FunctionDrawMode only: ends the frame renderFrame started.
    public static func endFrame() throws(NreError) { try call { nreEndFrame() } }
    public static func presentFrame() throws(NreError) { try call { nrePresentFrame() } }
    public static func submitFrame() throws(NreError) { try call { nreSubmitFrame() } }
    public static func windowShouldClose() throws(NreError) -> Bool { try get(false) { nreWindowShouldClose($0) } }
    public static func pollEvents() throws(NreError) { try call { nrePollEvents() } }

    // MARK: Shaders and pipelines

    /// Loads a compiled SPIR-V shader.
    public static func createShader(_ path: String, type: ShaderType) throws(NreError) -> Shader {
        Shader(try get(NreShader()) { nreCreateShader(path, type.c, $0) })
    }
    public static func destroyShader(_ shader: Shader) throws(NreError) { try call { nreDestroyShader(shader.c) } }
    public static func createPipeline(vertex: Shader, fragment: Shader) throws(NreError) -> Pipeline {
        Pipeline(try get(NrePipeline()) { nreCreatePipeline(vertex.c, fragment.c, $0) })
    }
    public static func destroyPipeline(_ pipeline: Pipeline) throws(NreError) { try call { nreDestroyPipeline(pipeline.c) } }
    public static func bindPipeline(_ pipeline: Pipeline) throws(NreError) { try call { nreBindPipeline(pipeline.c) } }

    // MARK: Models and instances

    /// Loads a model file (glTF/GLB, OBJ, ...). It isn't drawn by itself: create instances of it.
    public static func createModel(_ path: String) throws(NreError) -> Model {
        Model(try get(NreModel()) { nreCreateModel(path, $0) })
    }
    /// A model from your own triangles: counter-clockwise front faces, `indices` into `vertices` (three per
    /// triangle). Vertices with a `.zero` normal get one computed from their triangles. nil material = the default
    /// one. Draw it through instances like a loaded model. The arrays are copied by the renderer; `vertices` is
    /// passed to C without a copy (Vertex has NreVertex's layout).
    public static func createMeshModel(vertices: [Vertex], indices: [UInt32],
                                       material: Material? = nil) throws(NreError) -> Model {
        guard let vertexCount = UInt32(exactly: vertices.count), let indexCount = UInt32(exactly: indices.count) else {
            throw NreError(code: .invalidArgument, message: "createMeshModel(): more than UInt32.max vertices or indices")
        }
        func create(_ cVertices: UnsafePointer<NreVertex>?) throws(NreError) -> Model {
            Model(try get(NreModel()) { out in
                indices.withUnsafeBufferPointer {
                    nreCreateMeshModel(cVertices, vertexCount, $0.baseAddress, indexCount, material.c, out)
                }
            })
        }
        if Vertex.hasCLayout {
            return try vertices.withUnsafeBufferPointer { (buffer) throws(NreError) -> Model in
                try buffer.withMemoryRebound(to: NreVertex.self) { (c) throws(NreError) -> Model in
                    try create(c.baseAddress)
                }
            }
        }
        let converted = vertices.map(\.c)
        return try converted.withUnsafeBufferPointer { (c) throws(NreError) -> Model in try create(c.baseAddress) }
    }
    /// Instances of a destroyed model stay valid but draw nothing.
    public static func destroyModel(_ model: Model) throws(NreError) { try call { nreDestroyModel(model.c) } }
    public static func createInstance(_ model: Model) throws(NreError) -> Instance {
        Instance(try get(NreInstance()) { nreCreateInstance(model.c, $0) })
    }
    /// Also removes it from every scene it's in.
    public static func destroyInstance(_ instance: Instance) throws(NreError) { try call { nreDestroyInstance(instance.c) } }
    public static func setInstanceTransform(_ instance: Instance, _ transform: Transform) throws(NreError) {
        var t = transform.c
        try call { nreSetInstanceTransform(instance.c, &t) }
    }
    public static func getInstanceTransform(_ instance: Instance) throws(NreError) -> Transform {
        Transform(try get(NreTransform()) { nreGetInstanceTransform(instance.c, $0) })
    }
    public static func getInstanceMatrix(_ instance: Instance) throws(NreError) -> Mat4 {
        Mat4(try get(NreMat4()) { nreGetInstanceMatrix(instance.c, $0) })
    }
    public static func translateInstance(_ instance: Instance, _ offset: Vec3) throws(NreError) {
        try call { nreTranslateInstance(instance.c, offset.c) }
    }
    /// Angle in radians.
    public static func rotateInstance(_ instance: Instance, angle: Float, axis: Vec3) throws(NreError) {
        try call { nreRotateInstance(instance.c, angle, axis.c) }
    }
    public static func scaleInstance(_ instance: Instance, _ factor: Vec3) throws(NreError) {
        try call { nreScaleInstance(instance.c, factor.c) }
    }
    /// A hidden instance stays in its scenes but isn't drawn.
    public static func setVisibility(_ instance: Instance, _ visible: Bool) throws(NreError) {
        try call { nreSetVisibility(instance.c, visible) }
    }
    /// Whether the instance casts sun shadows (it still receives them). Default true; e.g. off for grass.
    public static func setCastsShadows(_ instance: Instance, _ castsShadows: Bool) throws(NreError) {
        try call { nreSetCastsShadows(instance.c, castsShadows) }
    }
    /// FunctionDrawMode only, between renderFrame and endFrame: draws the model once, no instance needed.
    public static func drawModel(_ model: Model, _ transform: Transform = .identity) throws(NreError) {
        var t = transform.c
        try call { nreDrawModel(model.c, &t) }
    }
    /// FunctionDrawMode only. Uses the instance's transform; a hidden instance isn't drawn.
    public static func drawInstance(_ instance: Instance) throws(NreError) { try call { nreDrawInstance(instance.c) } }

    // MARK: Scenes

    public static func createScene() throws(NreError) -> Scene { Scene(try get(NreScene()) { nreCreateScene($0) }) }
    public static func destroyScene(_ scene: Scene) throws(NreError) { try call { nreDestroyScene(scene.c) } }
    public static func bindScene(_ scene: Scene) throws(NreError) { try call { nreBindScene(scene.c) } }
    public static func getBoundScene() throws(NreError) -> Scene { Scene(try get(NreScene()) { nreGetBoundScene($0) }) }
    /// Adds to the bound scene. Adding an instance that's already in it does nothing.
    public static func addInstanceToScene(_ instance: Instance) throws(NreError) {
        try call { nreAddInstanceToScene(instance.c) }
    }
    public static func removeInstanceFromScene(_ instance: Instance) throws(NreError) {
        try call { nreRemoveInstanceFromScene(instance.c) }
    }

    // MARK: Textures and materials

    /// Loads an image file (PNG, JPEG, ...). `srgb`: true for color images (base color); false for data maps
    /// (normal and metallic-roughness maps).
    public static func createTexture(_ path: String, srgb: Bool = true) throws(NreError) -> Texture {
        Texture(try get(NreTexture()) { nreCreateTexture(path, srgb, $0) })
    }
    public static func destroyTexture(_ texture: Texture) throws(NreError) { try call { nreDestroyTexture(texture.c) } }
    /// nil textures use the default texture. metallicTexture and normalTexture are stored but not used yet.
    public static func createMaterial(baseTexture: Texture? = nil, metallicTexture: Texture? = nil,
                                      normalTexture: Texture? = nil) throws(NreError) -> Material {
        Material(try get(NreMaterial()) { nreCreateMaterial(baseTexture.c, metallicTexture.c, normalTexture.c, $0) })
    }
    /// Models still using it fall back to the default material.
    public static func destroyMaterial(_ material: Material) throws(NreError) { try call { nreDestroyMaterial(material.c) } }
    /// nil uses the default texture. Waits for the GPU, so don't call it every frame.
    public static func setMaterialBaseTexture(_ material: Material, _ texture: Texture?) throws(NreError) {
        try call { nreSetMaterialBaseTexture(material.c, texture.c) }
    }
    /// The model's material slots, in the file's material order. Empty for a model without materials (like OBJ).
    public static func getModelMaterials(_ model: Model) throws(NreError) -> [Material] {
        let count = try get(UInt32(0)) { nreGetModelMaterials(model.c, $0, nil) }
        if count == 0 { return [] }
        var materials = [NreMaterial](repeating: NreMaterial(), count: Int(count))
        var written = count
        try call { materials.withUnsafeMutableBufferPointer { nreGetModelMaterials(model.c, &written, $0.baseAddress) } }
        return materials.prefix(Int(written)).map(Material.init)
    }
    /// Makes every mesh of the model use material, replacing all of its slots.
    public static func setModelMaterial(_ model: Model, _ material: Material) throws(NreError) {
        try call { nreSetModelMaterial(model.c, material.c) }
    }
    /// Replaces slot `index` (an index into getModelMaterials).
    public static func replaceModelMaterial(_ model: Model, index: UInt32, _ material: Material) throws(NreError) {
        try call { nreReplaceModelMaterial(model.c, index, material.c) }
    }

    // MARK: Lights and the sun

    /// direction is the way the light travels (e.g. (0, -1, 0) shines straight down).
    public static func createDirectionalLight(_ direction: Vec3, color: Vec3 = .one,
                                              intensity: Float = 3) throws(NreError) -> Light {
        Light(try get(NreLight()) { nreCreateDirectionalLight(direction.c, color.c, intensity, $0) })
    }
    /// Falls off with 1/distance².
    public static func createPointLight(_ position: Vec3, color: Vec3 = .one,
                                        intensity: Float = 10) throws(NreError) -> Light {
        Light(try get(NreLight()) { nreCreatePointLight(position.c, color.c, intensity, $0) })
    }
    /// Also removes it from every scene it's in.
    public static func destroyLight(_ light: Light) throws(NreError) { try call { nreDestroyLight(light.c) } }
    public static func addLightToScene(_ light: Light) throws(NreError) { try call { nreAddLightToScene(light.c) } }
    public static func removeLightFromScene(_ light: Light) throws(NreError) { try call { nreRemoveLightFromScene(light.c) } }
    /// Only used by point lights.
    public static func setLightPosition(_ light: Light, _ position: Vec3) throws(NreError) {
        try call { nreSetLightPosition(light.c, position.c) }
    }
    /// Only used by directional lights.
    public static func setLightDirection(_ light: Light, _ direction: Vec3) throws(NreError) {
        try call { nreSetLightDirection(light.c, direction.c) }
    }
    public static func setLightColor(_ light: Light, _ color: Vec3, intensity: Float) throws(NreError) {
        try call { nreSetLightColor(light.c, color.c, intensity) }
    }
    /// The bound scene's sun: one directional light kept apart from the others (see renderer.h).
    public static func setSun(_ direction: Vec3, color: Vec3 = .one, intensity: Float = 3) throws(NreError) {
        try call { nreSetSun(direction.c, color.c, intensity) }
    }
    /// Keeps the color and intensity.
    public static func setSunDirection(_ direction: Vec3) throws(NreError) { try call { nreSetSunDirection(direction.c) } }
    public static func getSunDirection() throws(NreError) -> Vec3 { Vec3(try get(NreVec3()) { nreGetSunDirection($0) }) }

    // MARK: Scene lighting, shadows, haze, exposure and sky color (on the bound scene)

    /// Switches the bound scene's mode and resets shadow distance, haze and the sky to its defaults
    /// (small: shadows 20 m, haze off; landscape: shadows 500 m, haze 8000 m / 300 m). Keeps the sun, lights,
    /// exposure and sky color.
    public static func setSceneLighting(_ lighting: SceneLighting) throws(NreError) {
        try call { nreSetSceneLighting(lighting.c) }
    }
    /// Throws `.unknown` if libnre_c returns a mode these bindings don't know.
    public static func getSceneLighting() throws(NreError) -> SceneLighting {
        let c = try get(NreSceneLighting(rawValue: 0)) { nreGetSceneLighting($0) }
        guard let lighting = SceneLighting(c) else {
            throw NreError(code: .unknown, message: "getSceneLighting(): unknown NreSceneLighting \(c.rawValue)")
        }
        return lighting
    }
    /// How far from the camera the sun casts shadows, in meters. 0 turns sun shadows off; negative throws
    /// `.invalidArgument`.
    public static func setShadowDistance(_ meters: Float) throws(NreError) { try call { nreSetShadowDistance(meters) } }
    /// Distance haze. `visibility`: meters at which the haze hides almost everything, 0 = off. `height`: the haze
    /// layer's thickness in meters above y = 0, greater than 0.
    public static func setHaze(visibility: Float, height: Float = 300) throws(NreError) {
        try call { nreSetHaze(visibility, height) }
    }
    /// Greater than 0; 2 is twice as bright. Default 1.
    public static func setExposure(_ exposure: Float) throws(NreError) { try call { nreSetExposure(exposure) } }
    /// The background without a skybox (small), linear 0..1. Default (0.1, 0.3, 0.6).
    public static func setSkyColor(_ color: Vec3) throws(NreError) { try call { nreSetSkyColor(color.c) } }

    // MARK: Cubemaps and the skybox

    /// Six square images of the same size, in the OpenGL order: right (+X), left (-X), top (+Y), bottom (-Y),
    /// front (+Z), back (-Z).
    public static func createCubemap(right: String, left: String, top: String,
                                     bottom: String, front: String, back: String) throws(NreError) -> Cubemap {
        Cubemap(try get(NreCubemap()) { nreCreateCubemap(right, left, top, bottom, front, back, $0) })
    }
    /// Waits for the GPU. Scenes using it go back to having no skybox.
    public static func destroyCubemap(_ cubemap: Cubemap) throws(NreError) { try call { nreDestroyCubemap(cubemap.c) } }
    /// The bound scene's sky (3D only). nil removes it.
    public static func setSkybox(_ cubemap: Cubemap?) throws(NreError) { try call { nreSetSkybox(cubemap.c) } }

    // MARK: Camera (angles in radians)

    public static func setCameraPosition(_ position: Vec3) throws(NreError) { try call { nreSetCameraPosition(position.c) } }
    public static func getCameraPosition() throws(NreError) -> Vec3 { Vec3(try get(NreVec3()) { nreGetCameraPosition($0) }) }
    public static func moveCamera(_ offset: Vec3) throws(NreError) { try call { nreMoveCamera(offset.c) } }
    public static func moveCameraRelative(_ offset: Vec3) throws(NreError) { try call { nreMoveCameraRelative(offset.c) } }
    public static func setCameraRotation(yaw: Float, pitch: Float) throws(NreError) {
        try call { nreSetCameraRotation(yaw, pitch) }
    }
    public static func rotateCamera(yawDelta: Float, pitchDelta: Float) throws(NreError) {
        try call { nreRotateCamera(yawDelta, pitchDelta) }
    }
    public static func cameraLookAt(_ target: Vec3) throws(NreError) { try call { nreCameraLookAt(target.c) } }
    public static func getCameraForward() throws(NreError) -> Vec3 { Vec3(try get(NreVec3()) { nreGetCameraForward($0) }) }
    public static func getViewMatrix() throws(NreError) -> Mat4 { Mat4(try get(NreMat4()) { nreGetViewMatrix($0) }) }
    /// Vulkan conventions: clip-space +Y down, reverse-Z with an infinite far plane (see renderer.h).
    public static func getProjectionMatrix() throws(NreError) -> Mat4 {
        Mat4(try get(NreMat4()) { nreGetProjectionMatrix($0) })
    }
    /// The nearest visible instance of the bound scene under a pixel (getMousePosition units), or nil.
    /// render3D only.
    public static func pickInstance(at windowPos: Vec2) throws(NreError) -> RayHit? {
        var hit = false
        let rayHit = try get(NreRayHit()) { nrePickInstance(windowPos.c, &hit, $0) }
        return hit ? RayHit(rayHit) : nil
    }

    // MARK: Timing

    /// Frames per second, averaged over the last half second.
    public static func getFPS() throws(NreError) -> Float { try get(Float(0)) { nreGetFPS($0) } }
    /// Seconds between the last two presented frames.
    public static func getDeltaTime() throws(NreError) -> Float { try get(Float(0)) { nreGetDeltaTime($0) } }

    // MARK: Input (deltas cover the time between the last two pollEvents calls)

    public static func isMouseButtonDown(_ button: MouseButton) throws(NreError) -> Bool {
        try get(false) { nreIsMouseButtonDown(button.c, $0) }
    }
    /// True for the one frame after the pollEvents call that saw it go down.
    public static func isMouseButtonPressed(_ button: MouseButton) throws(NreError) -> Bool {
        try get(false) { nreIsMouseButtonPressed(button.c, $0) }
    }
    public static func isMouseButtonReleased(_ button: MouseButton) throws(NreError) -> Bool {
        try get(false) { nreIsMouseButtonReleased(button.c, $0) }
    }
    /// Pixels; +x is right, +y is down.
    public static func getMouseDelta() throws(NreError) -> Vec2 { Vec2(try get(NreVec2()) { nreGetMouseDelta($0) }) }
    /// Scroll wheel steps; positive is up / away from you.
    public static func getScrollDelta() throws(NreError) -> Float { try get(Float(0)) { nreGetScrollDelta($0) } }
    /// Pixels from the window's top-left corner, as of the last pollEvents call.
    public static func getMousePosition() throws(NreError) -> Vec2 { Vec2(try get(NreVec2()) { nreGetMousePosition($0) }) }
    public static func setCursorMode(_ mode: CursorMode) throws(NreError) { try call { nreSetCursorMode(mode.c) } }
    public static func isKeyDown(_ key: Key) throws(NreError) -> Bool { try get(false) { nreIsKeyDown(key.c, $0) } }
    public static func isKeyPressed(_ key: Key) throws(NreError) -> Bool { try get(false) { nreIsKeyPressed(key.c, $0) } }
    public static func isKeyReleased(_ key: Key) throws(NreError) -> Bool { try get(false) { nreIsKeyReleased(key.c, $0) } }
}
