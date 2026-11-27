//! Headless checks: no window is opened (init() is never called).
const std = @import("std");
const nre = @import("nre");
const c = nre.c;

test "layouts match nre.h" {
    try std.testing.expectEqual(8, @sizeOf(nre.Vec2));
    try std.testing.expectEqual(12, @sizeOf(nre.Vec3));
    try std.testing.expectEqual(16, @sizeOf(nre.Quat));
    try std.testing.expectEqual(64, @sizeOf(nre.Mat4));
    try std.testing.expectEqual(40, @sizeOf(nre.Transform));
    inline for (.{ nre.Shader, nre.Pipeline, nre.Model, nre.Instance, nre.Light, nre.Scene, nre.Texture, nre.Material, nre.Cubemap }) |H| {
        try std.testing.expectEqual(8, @sizeOf(H));
    }
    try std.testing.expectEqual(@sizeOf(c.NreTransform), @sizeOf(nre.Transform));
    try std.testing.expectEqual(@offsetOf(c.NreTransform, "rotation"), @offsetOf(nre.Transform, "rotation"));
    try std.testing.expectEqual(@offsetOf(c.NreTransform, "scale"), @offsetOf(nre.Transform, "scale"));
    try std.testing.expectEqual(@sizeOf(c.NreErrorCode), @sizeOf(nre.ErrorCode));
    try std.testing.expectEqual(@sizeOf(c.NreSceneLighting), @sizeOf(nre.SceneLighting));

    try std.testing.expectEqual(32, @sizeOf(nre.Vertex));
    try std.testing.expectEqual(0, @offsetOf(nre.Vertex, "position"));
    try std.testing.expectEqual(12, @offsetOf(nre.Vertex, "normal"));
    try std.testing.expectEqual(24, @offsetOf(nre.Vertex, "uv"));
    try std.testing.expectEqual(@sizeOf(c.NreVertex), @sizeOf(nre.Vertex));

    try std.testing.expectEqual(24, @sizeOf(nre.RayHit));
    try std.testing.expectEqual(0, @offsetOf(nre.RayHit, "instance"));
    try std.testing.expectEqual(8, @offsetOf(nre.RayHit, "distance"));
    try std.testing.expectEqual(12, @offsetOf(nre.RayHit, "point"));
    try std.testing.expectEqual(@sizeOf(c.NreRayHit), @sizeOf(nre.RayHit));
}

test "enum values match nre.h" {
    try std.testing.expectEqual(c.NRE_SCENE_LIGHTING_SMALL, @backingInt(nre.SceneLighting.small));
    try std.testing.expectEqual(c.NRE_SCENE_LIGHTING_LANDSCAPE, @backingInt(nre.SceneLighting.landscape));
    try std.testing.expectEqual(c.NRE_ERROR_INVALID_ARGUMENT, @backingInt(nre.ErrorCode.invalid_argument));
    try std.testing.expectEqual(c.NRE_ERROR_LIBRARY_LOAD_FAILED, @backingInt(nre.ErrorCode.library_load_failed));
    // Lighting modes newer than the bindings are a `_` value, not UB.
    const future: nre.SceneLighting = @fromBackingInt(@intCast(7));
    try std.testing.expect(future != .small and future != .landscape);
}

test "math helpers and defaults" {
    const t: nre.Transform = .{};
    try std.testing.expectEqual(nre.Quat.identity, t.rotation);
    try std.testing.expectEqual(nre.Vec3.one, t.scale);
    const v = nre.Vec3.init(1, 2, 3).add(.{ .x = 1 }).sub(.{ .y = 1 }).scale(2);
    try std.testing.expectEqual(nre.Vec3.init(4, 2, 6), v);
    try std.testing.expect(nre.Model.none.isNone());
}

test "version matches the bindings" {
    try std.testing.expectEqual(0, nre.abi_major);
    try std.testing.expectEqual(3, nre.abi_minor);
    const v = nre.version();
    try std.testing.expectEqual(nre.abi_major, v.major);
    try std.testing.expectEqual(nre.abi_minor, v.minor);
    try nre.checkVersion();
}

test "error codes" {
    try std.testing.expectEqualStrings("Ok", nre.errorCodeToString(.ok));
    try std.testing.expectEqualStrings("FileNotFound", nre.errorCodeToString(.file_not_found));
    try std.testing.expectEqualStrings("NotInitialized", nre.ErrorCode.not_initialized.toString());
    try std.testing.expectEqual(error.InvalidHandle, nre.ErrorCode.invalid_handle.toError().?);
    try std.testing.expectEqual(null, nre.ErrorCode.ok.toError());
    try std.testing.expectEqualStrings("InvalidArgument", nre.errorCodeToString(.invalid_argument));
    try std.testing.expectEqual(error.InvalidArgument, nre.ErrorCode.invalid_argument.toError().?);
    // Codes newer than the bindings map to Unknown, not UB.
    const future: nre.ErrorCode = @fromBackingInt(@intCast(1234));
    try std.testing.expectEqual(error.Unknown, future.toError().?);
    const huge: nre.ErrorCode = @fromBackingInt(@intCast(std.math.maxInt(c_int)));
    try std.testing.expectEqual(error.Unknown, huge.toError().?);
}

// One test, in order: the renderer's global state carries over between these steps.
// (Calling e.g. createModel before setRenderConfig aborts the process inside the renderer, so it isn't tested.)
test "error path after setRenderConfig, before init" {
    try nre.setRenderConfig(320, 240, "nre zig test");

    // Empty handle: InvalidHandle, with a message. (destroyXxx(.none) is a no-op that succeeds, like C++.)
    try nre.destroyModel(.none);
    try std.testing.expectError(error.InvalidHandle, nre.getInstanceTransform(.none));
    try std.testing.expectEqual(nre.ErrorCode.invalid_handle, nre.lastErrorCode());
    const msg = nre.lastErrorMessage();
    try std.testing.expect(msg.len > 0);
    try std.testing.expectEqualStrings(std.mem.span(c.nreGetLastErrorMessage()), msg);

    // A missing file: FileNotFound (checked before the renderer needs init()).
    try std.testing.expectError(error.FileNotFound, nre.createModel("/nonexistent/nre-zig-test.glb"));
    try std.testing.expectEqual(nre.ErrorCode.file_not_found, nre.lastErrorCode());
    try std.testing.expect(nre.lastErrorMessage().len > 0);

    // A second setRenderConfig: AlreadyInitialized.
    try std.testing.expectError(error.AlreadyInitialized, nre.setRenderConfig(320, 240, "again"));
    try std.testing.expectEqual(nre.ErrorCode.already_initialized, nre.lastErrorCode());
    try std.testing.expect(nre.lastErrorMessage().len > 0);
}
