#!/usr/bin/env bash
# Builds the portable Linux x86_64 release of NRE (librenderer + libnre_c) in a Docker container and packages it.
#
#   scripts/release/build-linux.sh [--debug] <output-dir> [<work-dir>]
#
#   <output-dir>  gets nre-<version>-linux-x86_64.tar.gz (+ .sha256)
#   <work-dir>    build tree, fetched deps and the unpacked package (default: <output-dir>/linux-work)
#   --debug       Debug build instead (misuse checks, Vulkan validation), not stripped, in <work-dir>/build-debug:
#                 nre-<version>-linux-x86_64-debug.tar.gz with the same file names and exports as the Release one
#
# The build runs in Ubuntu 24.04 (glibc 2.39) with GCC 15 from the toolchain PPA (see Dockerfile.linux), and links
# libstdc++/libgcc statically (their symbols hidden: --exclude-libs,ALL for libnre_c too), so the libraries load on
# any distro with glibc >= 2.38 whatever its libstdc++. The source tree is mounted read-only at /src and built in
# /build, so the generated renderer_version.h holds no host paths. Needs docker; everything else is in the image.
set -euo pipefail

BUILD_TYPE=Release
args=()
for arg in "$@"; do
    case "$arg" in
        --debug) BUILD_TYPE=Debug ;;
        *)       args+=("$arg") ;;
    esac
done
set -- "${args[@]}"
if [[ $# -lt 1 || $# -gt 2 ]]; then
    sed -n '2,14p' "$0" | sed 's/^# \{0,1\}//'
    exit 2
fi

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
SRC_DIR=$(cd "$SCRIPT_DIR/../.." && pwd)
mkdir -p "$1"
OUT_DIR=$(cd "$1" && pwd)
mkdir -p "${2:-$OUT_DIR/linux-work}"
WORK_DIR=$(cd "${2:-$OUT_DIR/linux-work}" && pwd)

VERSION=$(sed -nE 's/^project\([A-Za-z_]+ VERSION ([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' "$SRC_DIR/CMakeLists.txt")
[[ -n "$VERSION" ]] || { echo "error: no project(... VERSION x.y.z) in CMakeLists.txt" >&2; exit 1; }
if [[ $BUILD_TYPE == Debug ]]; then
    PKG=nre-$VERSION-linux-x86_64-debug
    BUILD_DIR=build-debug
else
    PKG=nre-$VERSION-linux-x86_64
    BUILD_DIR=build
fi
IMAGE=nre-release-linux:ubuntu24.04

echo "== NRE $VERSION: building image $IMAGE"
docker build -t "$IMAGE" -f "$SCRIPT_DIR/Dockerfile.linux" "$SCRIPT_DIR"

mkdir -p "$WORK_DIR/$BUILD_DIR"
echo "== $BUILD_TYPE build in container (source $SRC_DIR -> /src, build $WORK_DIR/$BUILD_DIR -> /build)"
docker run --rm \
    --user "$(id -u):$(id -g)" \
    -e HOME=/tmp -e PKG="$PKG" -e VERSION="$VERSION" -e BUILD_TYPE="$BUILD_TYPE" \
    -v "$SRC_DIR":/src:ro \
    -v "$WORK_DIR/$BUILD_DIR":/build \
    -v "$WORK_DIR":/work \
    "$IMAGE" bash -euo pipefail -c '
cmake -S /src -B /build -G Ninja \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DNRE_UNSAFE_API=OFF \
    -DNRE_C_API=ON \
    -DCMAKE_PREFIX_PATH="/opt/vulkan;/opt/glfw" \
    -DVulkan_INCLUDE_DIR=/opt/vulkan/include \
    -DVulkan_LIBRARY=/usr/lib/x86_64-linux-gnu/libvulkan.so.1 \
    -DCMAKE_DISABLE_FIND_PACKAGE_simdjson=ON \
    -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON \
    "-DCMAKE_SHARED_LINKER_FLAGS=-static-libstdc++ -static-libgcc -Wl,--exclude-libs,ALL"
cmake --build /build --target renderer nre_c

pkg=/work/$PKG
rm -rf "$pkg"
mkdir -p "$pkg/include" "$pkg/lib" "$pkg/shaders/include"
cp /src/include/nre.h /src/include/renderer.h /src/include/renderer_dynamic.h /src/include/renderer_functions.inl \
   /build/include/renderer_export.h "/build/version/$BUILD_TYPE/renderer_version.h" "$pkg/include/"
cp -r /src/shaders/include/nre "$pkg/shaders/include/"
# The real files plus their SONAME / dev symlinks (cp -P keeps links as links).
cp -P /build/bin/librenderer.so* /build/bin/libnre_c.so* "$pkg/lib/"
# Debug keeps its symbols and debug info.
if [ "$BUILD_TYPE" = Release ]; then
    strip --strip-unneeded "$pkg/lib/librenderer.so.$VERSION" "$pkg/lib/libnre_c.so.$VERSION"
fi
if [ -f /src/LICENSE ]; then cp /src/LICENSE "$pkg/"; fi

# Guard: only the allowed NEEDED entries (Vulkan loader, glibc, librenderer) and no libstdc++ symbol versions.
for f in "$pkg/lib/librenderer.so.$VERSION" "$pkg/lib/libnre_c.so.$VERSION"; do
    bad=$(objdump -p "$f" | awk "/NEEDED/{print \$2}" \
          | grep -vxE "libvulkan\.so\.1|lib(c|m)\.so\.6|libdl\.so\.2|libpthread\.so\.0|ld-linux-x86-64\.so\.2|librenderer\.so\.[0-9.]+" || true)
    if [ -n "$bad" ]; then echo "error: $f needs $bad" >&2; exit 1; fi
    if objdump -T "$f" | grep -qE "GLIBCXX_|CXXABI_"; then echo "error: $f needs libstdc++" >&2; exit 1; fi
done
# The newest glibc symbol version used: the minimum glibc, for the README.
objdump -T "$pkg"/lib/*.so.$VERSION | grep -oE "GLIBC_[0-9.]+" | sort -uV | tail -1 | cut -d_ -f2 > "/work/$PKG.glibc-min"
'

GLIBC_MIN=$(cat "$WORK_DIR/$PKG.glibc-min")
TITLE="NRE $VERSION - Linux x86_64"
DEBUG_NOTE=""
if [[ $BUILD_TYPE == Debug ]]; then
    TITLE="$TITLE (Debug build)"
    DEBUG_NOTE="
This is the Debug build: it checks for API misuse (wrong order of calls, bad arguments, stale
handles) and reports it as an error, and turns on the Vulkan validation layers when they're
installed (it runs without them too). It's slower than the Release build. Its libraries have the
same file names, SONAMEs and exported API (every NRE:: and nre* symbol) as the Release ones, so
they're drop-in replacements: swap lib/ to switch between them, without rebuilding the app.
"
fi
echo "== README (needs glibc >= $GLIBC_MIN)"
cat > "$WORK_DIR/$PKG/README.txt" <<EOF
$TITLE
$(printf '%s' "$TITLE" | tr '[:print:]' '=')

A Vulkan renderer: librenderer (C++ API) and libnre_c (C API over it).
$DEBUG_NOTE
Contents
  include/          nre.h (C API); renderer.h, renderer_dynamic.h, renderer_functions.inl,
                    renderer_export.h, renderer_version.h (C++ API)
  lib/              libnre_c.so.$VERSION, librenderer.so.$VERSION and their SONAME (.so.${VERSION%.*})
                    and development (.so) symlinks. libnre_c finds librenderer next to itself.
  shaders/include/  nre/frame.glsl, nre/lighting.glsl: includes for your own shaders
                    (glslc -I shaders/include, then #include "nre/lighting.glsl").

Requirements
  - Linux x86_64 with glibc >= $GLIBC_MIN (e.g. Ubuntu 24.04+, Debian 13+, Fedora 39+, current Arch).
    libstdc++ and libgcc are linked in statically, so the system's version doesn't matter.
  - A GPU driver with Vulkan 1.3 and the Vulkan loader (libvulkan.so.1).
  - X11 or Wayland (libX11 / libwayland-client / libxkbcommon are loaded at runtime).
  GLFW is linked in statically; nothing else is needed.

Using the C API
  cc app.c -I<nre>/include -L<nre>/lib -lnre_c -Wl,-rpath,<nre>/lib
  (or ship libnre_c.so.${VERSION%.*} and librenderer.so.${VERSION%.*} next to your app and use -Wl,-rpath,'\$ORIGIN').

Using the C++ API
  Include renderer.h and link librenderer (-lrenderer). Compile as C++23 or newer, with the glm headers
  (https://github.com/g-truc/glm, 1.0.1) on the include path and GLM_FORCE_DEPTH_ZERO_TO_ONE and
  GLM_FORCE_RADIANS defined:
  c++ -std=c++23 -DGLM_FORCE_DEPTH_ZERO_TO_ONE -DGLM_FORCE_RADIANS -I<nre>/include -I<glm> app.cpp \\
      -L<nre>/lib -lrenderer -Wl,-rpath,<nre>/lib

Language bindings
  The bindings in the repository's bindings/ folder load libnre_c from the directory in the
  NRE_LIB_DIR environment variable, e.g. NRE_LIB_DIR=<nre>/lib.
EOF

echo "== archive"
tar -C "$WORK_DIR" --owner=0 --group=0 --numeric-owner -czf "$OUT_DIR/$PKG.tar.gz" "$PKG"
(cd "$OUT_DIR" && sha256sum "$PKG.tar.gz" > "$PKG.tar.gz.sha256")
echo "== done: $OUT_DIR/$PKG.tar.gz"
cat "$OUT_DIR/$PKG.tar.gz.sha256"
