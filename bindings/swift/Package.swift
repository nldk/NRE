// swift-tools-version:6.0
// Swift bindings for NRE's C API (include/nre.h). Swift 6 language mode.
//
// Linking libnre_c: if NRE_LIB_DIR is set, every product links with `-L $NRE_LIB_DIR -rpath $NRE_LIB_DIR`
// (SwiftPM re-evaluates the manifest when the variable changes); otherwise the linker and loader search the
// usual system paths. unsafeFlags are only allowed in a root package, so when this package is a dependency
// pass the directory yourself: `swift build -Xlinker -L<dir> -Xlinker -rpath -Xlinker <dir>`. See README.md.
import PackageDescription

let libDir = Context.environment["NRE_LIB_DIR"].flatMap { $0.isEmpty ? nil : $0 }
let linkFlags: [LinkerSetting] = libDir.map { [.unsafeFlags(["-L", $0, "-Xlinker", "-rpath", "-Xlinker", $0])] } ?? []

let package = Package(
    name: "Nre",
    products: [
        .library(name: "Nre", targets: ["Nre"]),
        .executable(name: "OrbitExample", targets: ["OrbitExample"]),
    ],
    targets: [
        .systemLibrary(name: "CNre", path: "Sources/CNre"),
        .target(name: "Nre", dependencies: ["CNre"], linkerSettings: linkFlags),
        .executableTarget(name: "OrbitExample", dependencies: ["Nre"]),
        .testTarget(name: "NreTests", dependencies: ["Nre", "CNre"]),
    ]
)
