// Port of example/main.cpp: the police car on the plains skybox, rotating, with an orbit camera
// (drag with the left mouse button to orbit, scroll to zoom).
//
// Assets come from <repo>/example/assets, shaders from $NRE_SHADER_DIR (default
// <repo>/cmake-build-debug/example/shaders). <repo> is found from this file's path, not the working directory.

import Foundation
import Nre

/// The orbit camera lives in the app, not the bindings.
struct OrbitCamera {
    var target = Vec3.zero
    var distance: Float = 3
    var yaw: Float = 0
    var pitch: Float = 0

    static let rotateSpeed: Float = 0.01
    static let zoomStep: Float = 0.9
    static let maxPitch: Float = 1.55

    @MainActor
    mutating func update() throws(NreError) {
        if try NRE.isMouseButtonDown(.left) {
            let d = try NRE.getMouseDelta()
            yaw -= d.x * Self.rotateSpeed
            pitch = min(max(pitch + d.y * Self.rotateSpeed, -Self.maxPitch), Self.maxPitch)
        }

        distance = max(distance * pow(Self.zoomStep, try NRE.getScrollDelta()), 0.1)

        let offset = Vec3(cos(pitch) * sin(yaw), sin(pitch), cos(pitch) * cos(yaw))
        try NRE.setCameraPosition(target + offset * distance)
        try NRE.cameraLookAt(target)
    }
}

let apiMode: ApiMode = .functionDrawMode  // or .sceneMode

/// <repo>, five levels up from bindings/swift/Sources/OrbitExample/OrbitExample.swift.
let repoDir = URL(fileURLWithPath: #filePath)
    .deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
    .deletingLastPathComponent().deletingLastPathComponent()
let assetDir = repoDir.appendingPathComponent("example/assets").path
let shaderDir = ProcessInfo.processInfo.environment["NRE_SHADER_DIR"]
    ?? repoDir.appendingPathComponent("cmake-build-debug/example/shaders").path

@main
struct OrbitExample {
    @MainActor
    static func main() {
        do {
            try run()
        } catch {
            FileHandle.standardError.write("OrbitExample: \(error)\n".data(using: .utf8)!)
            exit(1)
        }
    }

    @MainActor
    static func run() throws(NreError) {
        // main.cpp uses errorPolicy Ignore. Under Ignore most failures print the error and still return NRE_OK,
        // so `try` would never see them; Return (the default) makes them throw.
        try NRE.setRenderConfig(width: 500, height: 500, title: "test", apiMode: apiMode)
        try NRE.initialize()
        let model = try NRE.createModel("\(assetDir)/police.glb")
        let car = try NRE.createInstance(model)
        let vertex = try NRE.createShader("\(shaderDir)/vertex.vert.spv", type: .vertex)
        let fragment = try NRE.createShader("\(shaderDir)/fragment.frag.spv", type: .fragment)
        let pipeline = try NRE.createPipeline(vertex: vertex, fragment: fragment)
        try NRE.bindPipeline(pipeline)
        _ = try NRE.createScene()
        try NRE.addInstanceToScene(car)
        try NRE.translateInstance(car, Vec3(0, 0, -1))
        let dir = "\(assetDir)/skyboxes/plains/"
        let sky = try NRE.createCubemap(right: dir + "right.png", left: dir + "left.png", top: dir + "top.png",
                                        bottom: dir + "bottom.png", front: dir + "front.png", back: dir + "back.png")
        try NRE.setSkybox(sky)

        var camera = OrbitCamera()
        camera.target = try NRE.getInstanceTransform(car).position
        while try !NRE.windowShouldClose() {
            try NRE.pollEvents()
            try camera.update()
            switch apiMode {
            case .sceneMode:
                try NRE.renderFrame()
            case .functionDrawMode:
                try NRE.renderFrame()
                try NRE.drawInstance(car)
                try NRE.endFrame()
            }

            try NRE.rotateInstance(car, angle: 0.0001, axis: Vec3(0, 1, 0))
        }
        try NRE.shutdown()
    }
}
