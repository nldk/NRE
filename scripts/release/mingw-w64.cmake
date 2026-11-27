# CMake toolchain file: cross-compile for Windows x86_64 with MinGW-w64 GCC (UCRT, SEH, posix threads).
#
#   cmake -DCMAKE_TOOLCHAIN_FILE=scripts/release/mingw-w64.cmake ...
#
# Used by build-windows.sh for NRE's Windows release and for the static GLFW it links in.

set(CMAKE_SYSTEM_NAME      Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(_triple x86_64-w64-mingw32)
set(CMAKE_C_COMPILER   ${_triple}-gcc)
set(CMAKE_CXX_COMPILER ${_triple}-g++)
set(CMAKE_RC_COMPILER  ${_triple}-windres)
set(CMAKE_AR           ${_triple}-ar     CACHE FILEPATH "")
set(CMAKE_RANLIB       ${_triple}-ranlib CACHE FILEPATH "")
set(CMAKE_DLLTOOL      ${_triple}-dlltool CACHE FILEPATH "")

# Libraries and headers come from the MinGW sysroot (plus whatever CMAKE_FIND_ROOT_PATH / *_DIR the caller adds);
# programs (glslc) from the host.
set(CMAKE_FIND_ROOT_PATH /usr/${_triple})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# renderer.dll / nre_c.dll instead of librenderer.dll / libnre_c.dll (nre.h and the bindings load "nre_c.dll").
# Platform/Windows-GNU.cmake sets the "lib" prefix after the toolchain file is read, so this goes in a rules
# override file, which CMake reads after the platform files. Import libraries keep their prefix:
# librenderer.dll.a, libnre_c.dll.a.
set(CMAKE_USER_MAKE_RULES_OVERRIDE ${CMAKE_CURRENT_LIST_DIR}/mingw-w64-overrides.cmake)
