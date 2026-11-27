// Checks that need no window or GPU: run with `./swiftw test` (or `swift test` with libnre_c on the linker path).

import CNre
import Testing
@testable import Nre

/// <repo>, four levels up from bindings/swift/Tests/NreTests/HeadlessTests.swift.
let repoDir: String = {
    var parts = "\(#filePath)".split(separator: "/", omittingEmptySubsequences: false)
    parts.removeLast(5)
    return parts.joined(separator: "/")
}()

@Test func versionMatchesTheTargetedABI() throws {
    let v = NRE.version
    #expect(v.major == 0 && v.minor == 3, "loaded libnre_c \(v.major).\(v.minor).\(v.patch)")
    try NRE.checkVersion()
}

@Test func versionCheckAcceptsOnly0_3() {
    #expect(versionError(major: 0, minor: 3, patch: 0) == nil)
    #expect(versionError(major: 0, minor: 3, patch: 7) == nil)
    for (major, minor) in [(0, 2), (0, 4), (1, 3), (0, 0)] as [(UInt32, UInt32)] {
        let error = versionError(major: major, minor: minor, patch: 0)
        #expect(error?.code == .libraryLoadFailed, "\(major).\(minor) should be rejected")
    }
}

@Test func errorCodesToString() {
    #expect(NRE.errorCodeToString(.ok) == "Ok")
    #expect(NRE.errorCodeToString(.fileNotFound) == "FileNotFound")
    #expect(NRE.errorCodeToString(.notInitialized) == "NotInitialized")
    #expect(NRE.errorCodeToString(.invalidArgument) == "InvalidArgument")
    #expect("\(ErrorCode.invalidHandle)" == NRE.errorCodeToString(.invalidHandle))
}

@Test func unknownErrorCodesMapToUnknown() {
    #expect(ErrorCode(code: 9) == .notInitialized)
    #expect(ErrorCode(code: 30) == .invalidArgument)
    #expect(ErrorCode(code: 31) == .unknown)
    #expect(ErrorCode(code: 9999) == .unknown)
    #expect(ErrorCode(code: -1) == .unknown)
    #expect(ErrorCode(code: Int32.max) == .unknown)
}

@Test func enumValuesMatchNreH() {
    #expect(ErrorCode.libraryLoadFailed.c == NRE_ERROR_LIBRARY_LOAD_FAILED)
    #expect(ErrorCode.invalidHandle.c == NRE_ERROR_INVALID_HANDLE)
    #expect(ErrorCode.invalidArgument.c == NRE_ERROR_INVALID_ARGUMENT)
    #expect(SceneLighting.small.c == NRE_SCENE_LIGHTING_SMALL)
    #expect(SceneLighting.landscape.c == NRE_SCENE_LIGHTING_LANDSCAPE)
    #expect(SceneLighting.allCases.count == 2)
    #expect(ShaderType.vertex.c == NRE_SHADER_VERTEX)
    #expect(ApiMode.functionDrawMode.c == NRE_API_MODE_FUNCTION_DRAW)
    #expect(RenderErrorPolicy.panic.c == NRE_ERROR_POLICY_PANIC)
    #expect(CursorMode.locked.c == NRE_CURSOR_LOCKED)
    #expect(Key.keypadEqual.c == NRE_KEY_KP_EQUAL)
    #expect(Key.menu.c == NRE_KEY_MENU)
}

/// The error path, without a window: setRenderConfig only stores the config, init() is never called.
/// One test, because it changes the process's renderer state (the other tests don't touch it).
@MainActor
@Test func errorsCarryCodeAndMessage() throws {
    try NRE.setRenderConfig(width: 64, height: 64, title: "headless", errorPolicy: .return)

    func expectError(_ code: ErrorCode, _ body: () throws(NreError) -> Void) {
        do {
            try body()
            Issue.record("expected \(code), but the call succeeded")
        } catch {
            #expect(error.code == code)
            #expect(!error.message.isEmpty)
            #expect(error.message == NRE.lastErrorMessage())
        }
    }
    expectError(.alreadyInitialized) { () throws(NreError) in try NRE.setRenderConfig(width: 64, height: 64, title: "again") }
    // Not checked: createModel(<existing file>) between setRenderConfig and init. The renderer has no
    // NotInitialized check there and asserts inside VMA (null allocator), aborting the test process.
    expectError(.fileNotFound) { () throws(NreError) in _ = try NRE.createModel(repoDir + "/no-such-model.glb") }
    expectError(.invalidHandle) { () throws(NreError) in _ = try NRE.getInstanceTransform(.empty) }
    expectError(.invalidHandle) { () throws(NreError) in try NRE.translateInstance(.empty, Vec3(1, 0, 0)) }
}

/// NotInitialized: calling into the renderer before setRenderConfig. There's no context to read the error
/// policy from, so the renderer reports it and aborts the process (policy Panic) instead of returning it.
/// Checked in a child process (a swift-testing exit test).
@Test func callBeforeSetRenderConfigAbortsWithNotInitialized() async throws {
    let result = try await #require(processExitsWith: .failure, observing: [\.standardErrorContent]) {
        _ = try? await NRE.createModel("does-not-matter.glb")
    }
    let stderr = String(decoding: result.standardErrorContent, as: UTF8.self)
    #expect(stderr.contains("NotInitialized"), "stderr: \(stderr)")
}

/// A value coming back from C that these bindings don't know is nil, not UB.
@Test func unknownSceneLightingFromCIsNil() {
    #expect(SceneLighting(NRE_SCENE_LIGHTING_SMALL) == .small)
    #expect(SceneLighting(NRE_SCENE_LIGHTING_LANDSCAPE) == .landscape)
    #expect(SceneLighting(NreSceneLighting(rawValue: 2)) == nil)
    #expect(SceneLighting(NRE_SCENE_LIGHTING_MAX_ENUM) == nil)
}

@Test func cStructLayoutsMatchNreH() {
    #expect(MemoryLayout<NreVec2>.size == 8)
    #expect(MemoryLayout<NreVec3>.size == 12)
    #expect(MemoryLayout<NreQuat>.size == 16)
    #expect(MemoryLayout<NreMat4>.size == 64)
    #expect(MemoryLayout<NreTransform>.size == 40)
    #expect(MemoryLayout<NreTransform>.offset(of: \.position) == 0)
    #expect(MemoryLayout<NreTransform>.offset(of: \.rotation) == 12)
    #expect(MemoryLayout<NreTransform>.offset(of: \.scale) == 28)
    #expect(MemoryLayout<NreModel>.size == 8)
    #expect(MemoryLayout<NreCubemap>.size == 8)
    #expect(MemoryLayout<NreErrorCode>.size == 4)
    #expect(MemoryLayout<NreKey>.size == 4)
    #expect(MemoryLayout<NreSceneLighting>.size == 4)
    #expect(MemoryLayout<NreVertex>.size == 32)
    #expect(MemoryLayout<NreVertex>.offset(of: \.position) == 0)
    #expect(MemoryLayout<NreVertex>.offset(of: \.normal) == 12)
    #expect(MemoryLayout<NreVertex>.offset(of: \.uv) == 24)
    #expect(MemoryLayout<NreRayHit>.size == 24)
    #expect(MemoryLayout<NreRayHit>.offset(of: \.instance) == 0)
    #expect(MemoryLayout<NreRayHit>.offset(of: \.distance) == 8)
    #expect(MemoryLayout<NreRayHit>.offset(of: \.point) == 12)
}

/// createMeshModel passes [Vertex] to C as-is when this holds (it converts once otherwise).
@Test func vertexHasNreVertexLayout() {
    #expect(MemoryLayout<Vertex>.size == 32 && MemoryLayout<Vertex>.stride == 32)
    #expect(MemoryLayout<Vertex>.offset(of: \.position) == 0)
    #expect(MemoryLayout<Vertex>.offset(of: \.normal) == 12)
    #expect(MemoryLayout<Vertex>.offset(of: \.uv) == 24)
    #expect(Vertex.hasCLayout)

    let vertices = [Vertex(position: Vec3(1, 2, 3), normal: Vec3(4, 5, 6), uv: Vec2(7, 8)),
                    Vertex(position: Vec3(9, 10, 11))]
    let rebound = vertices.withUnsafeBufferPointer { $0.withMemoryRebound(to: NreVertex.self) { $0.map(Vertex.init) } }
    #expect(rebound == vertices)
}

@Test func swiftValuesRoundTripThroughC() {
    let t = Transform(position: Vec3(1, 2, 3), rotation: Quat(0.1, 0.2, 0.3, 0.9), scale: Vec3(4, 5, 6))
    #expect(Transform(t.c) == t)
    #expect(Transform.identity.c.rotation.w == 1 && Transform.identity.c.scale.y == 1)
    #expect(Vec3(SIMD3<Float>(1, 2, 3)) + Vec3(1, 1, 1) * 2 == Vec3(3, 4, 5))
    #expect(Model.empty.c.index == 0 && Model.empty.c.generation == 0)
    #expect(Optional<Cubemap>.none.c.index == 0 && Cubemap.empty.isEmpty)
    #expect(Optional<Material>.none.c.index == 0 && Optional<Material>.none.c.generation == 0)
    let v = Vertex(position: Vec3(1, 2, 3), uv: Vec2(0.5, 1))
    #expect(Vertex(v.c) == v && v.normal == .zero)
    let hit = RayHit(NreRayHit(instance: NreInstance(index: 3, generation: 7), distance: 2.5,
                               point: NreVec3(x: 0, y: 0, z: 0.5)))
    #expect(hit == RayHit(instance: Instance(index: 3, generation: 7), distance: 2.5, point: Vec3(0, 0, 0.5)))

    var m = NreMat4()
    withUnsafeMutableBytes(of: &m.m) { raw in
        let floats = raw.bindMemory(to: Float.self)
        for i in 0..<16 { floats[i] = Float(i) }
    }
    let mat = Mat4(m)
    #expect(mat.elements == (0..<16).map(Float.init))
    #expect(mat[1, 2] == 6)  // column 1, row 2
}
