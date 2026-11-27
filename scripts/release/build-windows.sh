#!/usr/bin/env bash
# Cross-compiles the Windows x86_64 release of NRE (renderer.dll + nre_c.dll) with MinGW-w64 GCC and packages it.
#
#   scripts/release/build-windows.sh <output-dir> [<work-dir>] [--debug]
#
#   <output-dir>  gets nre-<version>-windows-x86_64.zip (+ .sha256), or with --debug
#                 nre-<version>-windows-x86_64-debug.zip (+ .sha256)
#   <work-dir>    downloads, GLFW / Vulkan import lib, build trees (build/, build-debug/) and the unpacked
#                 package (default: <output-dir>/windows-work). Don't run two builds on one work dir at once.
#   --debug       CMAKE_BUILD_TYPE=Debug (misuse checks, Vulkan validation when the layers are installed),
#                 not stripped. Same DLL names and exports as the Release build, so its DLLs are drop-in.
#
# Host needs: x86_64-w64-mingw32-{gcc,g++,dlltool,objdump,strip} (UCRT, SEH, posix threads), cmake >= 3.28,
# ninja, glslc, llvm-lib, bwrap, curl, zip. The build runs in a bubblewrap sandbox with the source mounted
# read-only at /src and the build tree at /build, so the generated renderer_version.h and __FILE__ strings hold
# no host paths. GLFW 3.4 is built from source and linked in statically; renderer.dll links vulkan-1.dll (from
# the user's GPU driver) through an import library made from the Vulkan-Loader's vulkan-1.def.
set -euo pipefail

BUILD_TYPE=Release
args=()
for a in "$@"; do
    if [[ $a == --debug ]]; then BUILD_TYPE=Debug; else args+=("$a"); fi
done
set -- "${args[@]}"
if [[ $# -lt 1 || $# -gt 2 ]]; then
    sed -n '2,17p' "$0" | sed 's/^# \{0,1\}//'
    exit 2
fi

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
SRC_DIR=$(cd "$SCRIPT_DIR/../.." && pwd)
mkdir -p "$1"
OUT_DIR=$(cd "$1" && pwd)
mkdir -p "${2:-$OUT_DIR/windows-work}"
WORK_DIR=$(cd "${2:-$OUT_DIR/windows-work}" && pwd)

VERSION=$(sed -nE 's/^project\([A-Za-z_]+ VERSION ([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' "$SRC_DIR/CMakeLists.txt")
[[ -n "$VERSION" ]] || { echo "error: no project(... VERSION x.y.z) in CMakeLists.txt" >&2; exit 1; }
RELEASE_PKG=nre-$VERSION-windows-x86_64
if [[ $BUILD_TYPE == Debug ]]; then PKG=$RELEASE_PKG-debug BUILD_SUBDIR=build-debug; else PKG=$RELEASE_PKG BUILD_SUBDIR=build; fi
TRIPLE=x86_64-w64-mingw32

for tool in $TRIPLE-gcc $TRIPLE-g++ $TRIPLE-dlltool $TRIPLE-objdump $TRIPLE-strip cmake ninja glslc llvm-lib bwrap curl zip; do
    command -v "$tool" >/dev/null || { echo "error: $tool not found" >&2; exit 1; }
done

# ---------------------------------------------------------------------------
# Pinned downloads
# ---------------------------------------------------------------------------
VULKAN_TAG=v1.4.363   # Vulkan-Headers, Vulkan-Utility-Libraries (vk_enum_string_helper.h) and the loader's
                      # export list (vulkan-1.def), all of the same SDK
GLFW_VERSION=3.4
DOWNLOADS=(
    "glfw-$GLFW_VERSION.zip|https://github.com/glfw/glfw/releases/download/$GLFW_VERSION/glfw-$GLFW_VERSION.zip|b5ec004b2712fd08e8861dc271428f048775200a2df719ccf575143ba749a3e9"
    "Vulkan-Headers-$VULKAN_TAG.tar.gz|https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/$VULKAN_TAG.tar.gz|cbaf687d3c59b9666fe080e7f8396b6b4f4d344a768ee6b337c59852f4526b68"
    "Vulkan-Utility-Libraries-$VULKAN_TAG.tar.gz|https://github.com/KhronosGroup/Vulkan-Utility-Libraries/archive/refs/tags/$VULKAN_TAG.tar.gz|5e2ca5b71a81e78e6d35d882cfde06661c542d543272de4126fb8dd87c26f80a"
    "vulkan-1-$VULKAN_TAG.def|https://raw.githubusercontent.com/KhronosGroup/Vulkan-Loader/$VULKAN_TAG/loader/vulkan-1.def|9ba339b7f5ee2df28487698a6840ecf095fac58b415c2c89ad9f161e9d316378"
)
mkdir -p "$WORK_DIR/downloads"
for entry in "${DOWNLOADS[@]}"; do
    IFS='|' read -r name url sha <<<"$entry"
    file=$WORK_DIR/downloads/$name
    if [[ ! -f "$file" ]]; then
        echo "== downloading $url"
        curl -sSfL -o "$file.part" "$url"
        mv "$file.part" "$file"
    fi
    echo "$sha  $file" | sha256sum -c --quiet - || { echo "error: checksum mismatch for $name" >&2; exit 1; }
done

rm -rf "$WORK_DIR/deps-src"
mkdir -p "$WORK_DIR/deps-src"
(cd "$WORK_DIR/deps-src" && cmake -E tar xf "$WORK_DIR/downloads/glfw-$GLFW_VERSION.zip" \
                         && cmake -E tar xf "$WORK_DIR/downloads/Vulkan-Headers-$VULKAN_TAG.tar.gz" \
                         && cmake -E tar xf "$WORK_DIR/downloads/Vulkan-Utility-Libraries-$VULKAN_TAG.tar.gz")
cp "$WORK_DIR/downloads/vulkan-1-$VULKAN_TAG.def" "$WORK_DIR/deps-src/vulkan-1.def"

# ---------------------------------------------------------------------------
# Build in a sandbox: /src (source, read-only), /scripts (this folder, read-only), /build (NRE build tree),
# /work (everything else).
# ---------------------------------------------------------------------------
mkdir -p "$WORK_DIR/$BUILD_SUBDIR"
SANDBOX=(bwrap
    --ro-bind /usr /usr
    --symlink usr/bin /bin --symlink usr/bin /sbin --symlink usr/lib /lib --symlink usr/lib /lib64
    --ro-bind /etc /etc
    --proc /proc --dev /dev --tmpfs /tmp
    --ro-bind "$SRC_DIR" /src
    --ro-bind "$SCRIPT_DIR" /scripts
    --bind "$WORK_DIR/$BUILD_SUBDIR" /build
    --bind "$WORK_DIR" /work
    --chdir /work
    --unshare-user --unshare-ipc --unshare-pid --unshare-uts --die-with-parent
    --clearenv
    --setenv HOME /tmp --setenv PATH /usr/bin --setenv LANG C.UTF-8
    --setenv PKG "$PKG" --setenv VERSION "$VERSION" --setenv TRIPLE "$TRIPLE" --setenv VULKAN_TAG "$VULKAN_TAG"
    --setenv GLFW_VERSION "$GLFW_VERSION" --setenv BUILD_TYPE "$BUILD_TYPE"
)
echo "== $BUILD_TYPE build in sandbox (source $SRC_DIR -> /src, build $WORK_DIR/$BUILD_SUBDIR -> /build, work $WORK_DIR -> /work)"
"${SANDBOX[@]}" bash -euo pipefail -c '
TOOLCHAIN=/scripts/mingw-w64.cmake

echo "== GLFW $GLFW_VERSION (static)"
rm -rf /work/glfw-build /work/deps
cmake -S /work/deps-src/glfw-$GLFW_VERSION -B /work/glfw-build -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/work/deps/glfw \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DBUILD_SHARED_LIBS=OFF \
    -DGLFW_BUILD_EXAMPLES=OFF -DGLFW_BUILD_TESTS=OFF -DGLFW_BUILD_DOCS=OFF -DGLFW_INSTALL=ON
cmake --build /work/glfw-build
cmake --install /work/glfw-build

echo "== Vulkan headers $VULKAN_TAG (+ vk_enum_string_helper.h) + vulkan-1.dll import library"
mkdir -p /work/deps/vulkan/lib
cp -r /work/deps-src/Vulkan-Headers-${VULKAN_TAG#v}/include /work/deps/vulkan/include
cp /work/deps-src/Vulkan-Utility-Libraries-${VULKAN_TAG#v}/include/vulkan/vk_enum_string_helper.h /work/deps/vulkan/include/vulkan/
$TRIPLE-dlltool -d /work/deps-src/vulkan-1.def -D vulkan-1.dll -l /work/deps/vulkan/lib/libvulkan-1.dll.a

echo "== NRE $VERSION ($BUILD_TYPE)"
cmake -S /src -B /build -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DNRE_UNSAFE_API=OFF \
    -DNRE_C_API=ON \
    -Dglfw3_DIR=/work/deps/glfw/lib/cmake/glfw3 \
    -DVulkan_INCLUDE_DIR=/work/deps/vulkan/include \
    -DVulkan_LIBRARY=/work/deps/vulkan/lib/libvulkan-1.dll.a \
    -DVulkan_GLSLC_EXECUTABLE=/usr/bin/glslc \
    -DCMAKE_DISABLE_FIND_PACKAGE_simdjson=ON
cmake --build /build --target renderer nre_c

echo "== package"
pkg=/work/$PKG
rm -rf "$pkg"
mkdir -p "$pkg/include" "$pkg/bin" "$pkg/lib" "$pkg/shaders/include"
cp /src/include/nre.h /src/include/renderer.h /src/include/renderer_dynamic.h /src/include/renderer_functions.inl \
   /build/include/renderer_export.h /build/version/$BUILD_TYPE/renderer_version.h "$pkg/include/"
cp -r /src/shaders/include/nre "$pkg/shaders/include/"
cp /build/bin/renderer.dll /build/bin/nre_c.dll "$pkg/bin/"
# Debug keeps its symbols and DWARF debug info (gdb, or WinDbg / VS with a DWARF plugin).
if [ "$BUILD_TYPE" = Release ]; then $TRIPLE-strip --strip-unneeded "$pkg/bin/renderer.dll" "$pkg/bin/nre_c.dll"; fi
# One shared C++ runtime for renderer.dll, nre_c.dll and C++ apps: std::string / std::expected cross the DLL boundary.
for dll in libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll; do
    f=$($TRIPLE-g++ -print-file-name=$dll)
    [ -f "$f" ] || f=/usr/$TRIPLE/bin/$dll
    cp "$f" "$pkg/bin/"
done
cp /build/librenderer.dll.a /build/libnre_c.dll.a "$pkg/lib/"
# nre_c.def + nre_c.lib: the C API for MSVC / clang-cl link.exe users.
{
    echo "LIBRARY nre_c.dll"
    echo "EXPORTS"
    $TRIPLE-objdump -p "$pkg/bin/nre_c.dll" \
        | sed -n "/^\[Ordinal\/Name Pointer\] Table/,/^\$/p" | sed -nE "s/^\s*\[ *[0-9]+\].* (\S+)\$/    \1/p"
} > "$pkg/lib/nre_c.def"
llvm-lib /nologo /machine:x64 /def:"$pkg/lib/nre_c.def" /out:"$pkg/lib/nre_c.lib"
rm -f "$pkg/lib/nre_c.exp"
if [ -f /src/LICENSE ]; then cp /src/LICENSE "$pkg/"; fi
'

PKG_DIR=$WORK_DIR/$PKG
echo "== README"
TITLE="NRE $VERSION - Windows x86_64"
DEBUG_NOTE=
if [[ $BUILD_TYPE == Debug ]]; then
    TITLE="$TITLE (Debug build)"
    DEBUG_NOTE="

This is the Debug build, for development. It checks for API misuse (invalid arguments, calls before init()
or in the wrong API mode, out-of-range indices) and reports it as errors, where the Release build trusts the
caller. It also turns on the Vulkan validation layers when they're installed (e.g. with the Vulkan SDK) and
prints their messages; without them it runs without validation. It's slower than the Release build, and its
DLLs keep their debug info (DWARF, for gdb). The DLLs have the same names and exports as the Release ones,
so they're drop-in replacements: swap bin\\ to switch, no relinking."
fi
cat > "$PKG_DIR/README.txt" <<EOF
$TITLE
$(printf "%s" "$TITLE" | tr "[:print:]" "=")

A Vulkan renderer: renderer.dll (C++ API) and nre_c.dll (C API over it).
Built with MinGW-w64 GCC (UCRT, SEH exceptions, posix threads).$DEBUG_NOTE

Contents
  bin/              nre_c.dll, renderer.dll, and the GCC runtime they share: libstdc++-6.dll,
                    libgcc_s_seh-1.dll, libwinpthread-1.dll. Ship all five next to your .exe
                    (or put bin\\ on PATH): Windows looks for them next to the .exe and on PATH.
  lib/              nre_c.lib + nre_c.def    import library of the C API for MSVC / clang-cl
                    libnre_c.dll.a          import library of the C API for MinGW-w64
                    librenderer.dll.a       import library of the C++ API for MinGW-w64
  include/          nre.h (C API); renderer.h, renderer_dynamic.h, renderer_functions.inl,
                    renderer_export.h, renderer_version.h (C++ API)
  shaders/include/  nre/frame.glsl, nre/lighting.glsl: includes for your own shaders
                    (glslc -I shaders/include, then #include "nre/lighting.glsl").

Requirements
  - Windows 10 or 11, x86_64 (the Universal C Runtime is part of the OS).
  - A GPU driver with Vulkan 1.3; it installs the Vulkan loader (vulkan-1.dll).
  GLFW is linked in statically; nothing else is needed.

Using the C API (works from any C compiler or language FFI)
  MSVC:      cl app.c /I<nre>\\include /link <nre>\\lib\\nre_c.lib
  clang-cl:  clang-cl app.c /I<nre>\\include /link <nre>\\lib\\nre_c.lib
  MinGW-w64: gcc app.c -I<nre>/include -L<nre>/lib -lnre_c
  Then put the DLLs from bin\\ next to app.exe.

Using the C++ API (MinGW-w64 GCC only)
  renderer.h passes C++ types (std::string, std::expected, std::span, ...) across the DLL boundary, so the app
  must use the same C++ ABI and runtime: MinGW-w64 GCC with UCRT, SEH and posix threads (e.g. MSYS2 UCRT64),
  and run with the libstdc++-6.dll from bin\\ (GCC 16.2's; it also runs code built by older GCCs). MSVC and
  clang-cl use a different C++ ABI and can't use it; use the C API from them. Compile as C++23 or newer, with the glm headers (https://github.com/g-truc/glm,
  1.0.1) on the include path and GLM_FORCE_DEPTH_ZERO_TO_ONE and GLM_FORCE_RADIANS defined:
  g++ -std=c++23 -DGLM_FORCE_DEPTH_ZERO_TO_ONE -DGLM_FORCE_RADIANS -I<nre>/include -I<glm> app.cpp \\
      -L<nre>/lib -lrenderer

Language bindings
  The bindings in the repository's bindings/ folder load nre_c.dll from the directory in the NRE_LIB_DIR
  environment variable, e.g. set NRE_LIB_DIR=<nre>\\bin. Windows then finds renderer.dll and the runtime DLLs
  next to nre_c.dll only if the language loads it with LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR (Python's ctypes does);
  a plain LoadLibrary (e.g. LuaJIT's ffi.load) searches the .exe's folder and PATH instead, so also add
  <nre>\\bin to PATH (set PATH=<nre>\\bin;%PATH%). Bindings that link nre_c at build time (Rust, Go, Swift)
  need nre_c.dll next to the .exe or on PATH as well: Windows has no rpath.
EOF
sed -i 's/$/\r/' "$PKG_DIR/README.txt"

# ---------------------------------------------------------------------------
# Checks: exports, imports, no host paths.
# ---------------------------------------------------------------------------
echo "== checks"
exports() { $TRIPLE-objdump -p "$1" | sed -n '/^\[Ordinal\/Name Pointer\] Table/,/^$/p' | sed -nE 's/^\s*\[ *[0-9]+\].* (\S+)$/\1/p'; }
imports() { $TRIPLE-objdump -p "$1" | sed -nE 's/^\s*DLL Name: (\S+)$/\1/p'; }
fail=0
n_api=$(grep -cE '^\s*NRE_C_EXPORT ' "$SRC_DIR/include/nre.h")
n_exp=$(exports "$PKG_DIR/bin/nre_c.dll" | grep -c '^nre')
n_other=$(exports "$PKG_DIR/bin/nre_c.dll" | grep -vc '^nre' || true)
echo "nre_c.dll: $n_exp nre* exports (nre.h declares $n_api), $n_other others"
[[ $n_exp -eq $n_api && $n_other -eq 0 ]] || fail=1
# NRE:: symbols, plus renderer_dynamic.h's extern "C" nreGetFunctionTable.
n_bad=$(exports "$PKG_DIR/bin/renderer.dll" | grep -vcE '^_Z(N|NK|TSN|TIN|TVN)3NRE|^nreGetFunctionTable$' || true)
echo "renderer.dll: $(exports "$PKG_DIR/bin/renderer.dll" | wc -l) exports, $n_bad outside NRE::"
[[ $n_bad -eq 0 ]] || { exports "$PKG_DIR/bin/renderer.dll" | grep -vE '^_Z(N|NK|TSN|TIN|TVN)3NRE|^nreGetFunctionTable$' | head; fail=1; }
allowed='^(KERNEL32|USER32|GDI32|SHELL32|msvcrt|vulkan-1|libstdc\+\+-6|libgcc_s_seh-1|libwinpthread-1|renderer)\.dll$|^api-ms-win-crt-[a-z]+-l1-1-0\.dll$'
for dll in renderer.dll nre_c.dll; do
    echo "$dll imports: $(imports "$PKG_DIR/bin/$dll" | tr '\n' ' ')"
    bad=$(imports "$PKG_DIR/bin/$dll" | grep -viE "$allowed" || true)
    [[ -z "$bad" ]] || { echo "error: $dll imports $bad" >&2; fail=1; }
done
if [[ $BUILD_TYPE == Debug ]]; then
    if [[ -d $WORK_DIR/$RELEASE_PKG/bin ]]; then
        for dll in renderer.dll nre_c.dll; do
            if diff <(exports "$WORK_DIR/$RELEASE_PKG/bin/$dll" | sort) <(exports "$PKG_DIR/bin/$dll" | sort) >/dev/null; then
                echo "$dll: same exports as the Release build"
            else
                echo "error: $dll exports differ from the Release build" >&2; fail=1
            fi
        done
    else
        echo "note: no Release package in $WORK_DIR, export lists not compared with it"
    fi
fi
if grep -rIlaE 'niel|claude' "$PKG_DIR"; then echo "error: host paths in the package" >&2; fail=1; fi
grep -H NRE_LIBRARY "$PKG_DIR/include/renderer_version.h"
[[ $fail -eq 0 ]] || { echo "error: checks failed" >&2; exit 1; }

echo "== archive"
rm -f "$OUT_DIR/$PKG.zip"
(cd "$WORK_DIR" && TZ=UTC zip -qrX "$OUT_DIR/$PKG.zip" "$PKG")
(cd "$OUT_DIR" && sha256sum "$PKG.zip" > "$PKG.zip.sha256")
echo "== done: $OUT_DIR/$PKG.zip"
cat "$OUT_DIR/$PKG.zip.sha256"
