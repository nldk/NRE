//! Zig bindings for NRE's C API (include/nre.h, libnre_c).
//!
//!     zig build test   headless checks (no window)
//!     zig build run    the orbit-camera example (opens a window)
//!
//! Options:
//!     -Dnre-lib-dir=<dir>  where libnre_c lives (default: env NRE_LIB_DIR, else the linker's normal search).
//!                          The example and tests get it as an rpath, so they run without LD_LIBRARY_PATH.
const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const lib_dir: ?[]const u8 = b.option([]const u8, "nre-lib-dir", "Directory containing libnre_c (default: env NRE_LIB_DIR)") orelse blk: {
        // Reading the environment at configure time: tell the build cache it can't track this.
        b.graph.poisonCache();
        break :blk b.graph.environ_map.get("NRE_LIB_DIR");
    };

    // Raw layer: nre.h translated as-is. Exposed as `nre.c`.
    const translate = b.addTranslateC(.{
        .root_source_file = b.path("../../include/nre.h"),
        .target = target,
        .optimize = optimize,
    });
    const c_mod = translate.createModule();
    c_mod.linkSystemLibrary("nre_c", .{});
    if (lib_dir) |dir| c_mod.addLibraryPath(b.graph.cwdRelativePath(dir));

    // Idiomatic layer: the public `nre` module.
    const nre = b.addModule("nre", .{
        .root_source_file = b.path("src/nre.zig"),
        .target = target,
        .optimize = optimize,
        .imports = &.{.{ .name = "c", .module = c_mod }},
    });

    // Example: examples/orbit.zig. Like the C++ example's ASSET_DIR/SHADER_DIR, it gets the repo root
    // at compile time, so it doesn't depend on the working directory.
    const options = b.addOptions();
    options.addOption([]const u8, "repo_root", b.pathResolve(&.{ b.root.toString(b.graph.arena) catch @panic("OOM"), "..", ".." }));
    const example = b.addExecutable(.{
        .name = "orbit",
        .root_module = b.createModule(.{
            .root_source_file = b.path("examples/orbit.zig"),
            .target = target,
            .optimize = optimize,
            .imports = &.{
                .{ .name = "nre", .module = nre },
                .{ .name = "build_options", .module = options.createModule() },
            },
        }),
    });
    if (lib_dir) |dir| example.root_module.addRPath(b.graph.cwdRelativePath(dir));
    b.installArtifact(example);

    const run_cmd = b.addRunArtifact(example);
    run_cmd.addPassthruArgs();
    const run_step = b.step("run", "Run the orbit-camera example");
    run_step.dependOn(&run_cmd.step);

    // Tests: tests/headless.zig (no window needed).
    const tests = b.addTest(.{
        .root_module = b.createModule(.{
            .root_source_file = b.path("tests/headless.zig"),
            .target = target,
            .optimize = optimize,
            .imports = &.{.{ .name = "nre", .module = nre }},
        }),
    });
    if (lib_dir) |dir| tests.root_module.addRPath(b.graph.cwdRelativePath(dir));
    const test_step = b.step("test", "Run the headless tests");
    test_step.dependOn(&b.addRunArtifact(tests).step);
}
