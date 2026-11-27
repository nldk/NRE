//! Port of example/main.cpp: the police car on the plains skybox, rotating, with an orbit camera
//! (drag with the left mouse button, scroll to zoom).
//!
//! Assets come from <repo>/example/assets; shaders from $NRE_SHADER_DIR, default
//! <repo>/cmake-build-debug/example/shaders. <repo> is baked in by build.zig (like the C++ ASSET_DIR).
const std = @import("std");
const nre = @import("nre");
const build_options = @import("build_options");

const Vec3 = nre.Vec3;

/// The camera controller lives in the app, not in the bindings.
const OrbitCamera = struct {
    target: Vec3 = .zero,
    distance: f32 = 3,
    yaw: f32 = 0,
    pitch: f32 = 0,

    const rotate_speed = 0.01;
    const zoom_step = 0.9;
    const max_pitch = 1.55;

    fn update(self: *OrbitCamera) nre.Error!void {
        if (try nre.isMouseButtonDown(.left)) {
            const d = try nre.getMouseDelta();
            self.yaw -= d.x * rotate_speed;
            self.pitch = std.math.clamp(self.pitch + d.y * rotate_speed, -max_pitch, max_pitch);
        }
        self.distance = @max(self.distance * std.math.pow(f32, zoom_step, try nre.getScrollDelta()), 0.1);

        const offset = Vec3.init(@cos(self.pitch) * @sin(self.yaw), @sin(self.pitch), @cos(self.pitch) * @cos(self.yaw));
        try nre.setCameraPosition(self.target.add(offset.scale(self.distance)));
        try nre.cameraLookAt(self.target);
    }
};

const api_mode: nre.ApiMode = .function_draw; // or .scene

pub fn main(init: std.process.Init) !void {
    const arena = init.arena.allocator();
    const repo = build_options.repo_root;
    const assets = try std.fs.path.join(arena, &.{ repo, "example", "assets" });
    const shaders = init.environ_map.get("NRE_SHADER_DIR") orelse
        try std.fs.path.join(arena, &.{ repo, "cmake-build-debug", "example", "shaders" });

    run(arena, assets, shaders) catch |err| {
        std.log.err("{t}: {s}", .{ err, nre.lastErrorMessage() });
        return err;
    };
}

fn path(arena: std.mem.Allocator, dir: []const u8, file: []const u8) ![:0]const u8 {
    return std.fmt.allocPrintSentinel(arena, "{s}/{s}", .{ dir, file }, 0);
}

fn run(arena: std.mem.Allocator, assets: []const u8, shaders: []const u8) !void {
    try nre.setRenderConfigFull(500, 500, "test", .render_3d, api_mode, .ignore, false, true, true);
    try nre.init();
    defer nre.shutdown() catch {};

    const model = try nre.createModel(try path(arena, assets, "police.glb"));
    const car = try nre.createInstance(model);
    const vertex = try nre.createShader(try path(arena, shaders, "vertex.vert.spv"), .vertex);
    const frag = try nre.createShader(try path(arena, shaders, "fragment.frag.spv"), .fragment);
    const pipeline = try nre.createPipeline(vertex, frag);
    try nre.bindPipeline(pipeline);
    _ = try nre.createScene();
    try nre.addInstanceToScene(car);
    try nre.translateInstance(car, .init(0, 0, -1));

    const sky_dir = try std.fs.path.join(arena, &.{ assets, "skyboxes", "plains" });
    const sky = try nre.createCubemap(
        try path(arena, sky_dir, "right.png"),
        try path(arena, sky_dir, "left.png"),
        try path(arena, sky_dir, "top.png"),
        try path(arena, sky_dir, "bottom.png"),
        try path(arena, sky_dir, "front.png"),
        try path(arena, sky_dir, "back.png"),
    );
    try nre.setSkybox(sky);

    var camera: OrbitCamera = .{ .target = (try nre.getInstanceTransform(car)).position };
    while (!try nre.windowShouldClose()) {
        try nre.pollEvents();
        try camera.update();
        switch (api_mode) {
            .scene => try nre.renderFrame(),
            .function_draw => {
                try nre.renderFrame();
                try nre.drawInstance(car);
                try nre.endFrame();
            },
        }
        try nre.rotateInstance(car, 0.0001, .init(0, 1, 0));
    }
}
