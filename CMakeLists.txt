cmake_minimum_required(VERSION 3.28)
# Bump on every release, together with a matching git tag (vX.Y.Z).
# While 0.x: bump the minor for breaking API/ABI changes, the patch for fixes.
project(VulkanRenderer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Only when building NRE itself: put the app and librenderer.so together in bin/.
# A project that fetches NRE keeps its own output layout.
if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
endif()

# OFF: NRE::Unsafe (renderer_unsafe.h) isn't compiled, so librenderer.so doesn't contain or export it.
# The *-unsafe presets in CMakePresets.json turn it on.
option(NRE_UNSAFE_API "Build and export NRE::Unsafe: raw Vulkan/GLFW handles from inside the renderer" OFF)

# ---------------------------------------------------------------------------
# System dependencies
# ---------------------------------------------------------------------------
find_package(Vulkan 1.3 REQUIRED)
find_package(glfw3 3.4 REQUIRED)

# ---------------------------------------------------------------------------
# Fetched dependencies
# ---------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
    vk_bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
    # Pinned so a configure never pulls in new upstream changes. A commit hash can't be fetched shallow.
    GIT_TAG        558b0024c4af3abffbf581fc06b9051bd9b64b93   # main, 2026-10-07
    SYSTEM
)

FetchContent_Declare(
    VulkanMemoryAllocator
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG        v3.3.0
    GIT_SHALLOW    TRUE
    SYSTEM
)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
    SYSTEM
)

FetchContent_Declare(
    fastgltf
    GIT_REPOSITORY https://github.com/spnda/fastgltf.git
    GIT_TAG        v0.9.1
    GIT_SHALLOW    TRUE
    SYSTEM
)

FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG        v2.0.0rc13
    GIT_SHALLOW    TRUE
    SYSTEM
)

# stb has no releases, so pin a commit. It's header-only with no CMakeLists;
# the stb_image target is defined below.
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        31c1ad37456438565541f4919958214b6e762fb4
)

set(VK_BOOTSTRAP_TEST OFF CACHE BOOL "" FORCE)

# GLM is header-only; skip its optional static library and tests.
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_TESTS   OFF CACHE BOOL "" FORCE)

# fastgltf (and the simdjson it downloads) are static libs that end up inside
# librenderer.so, so they must be position-independent too.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# vk-bootstrap is a static lib that ends up inside librenderer.so, so it
# must be compiled as position-independent code.
set(VK_BOOTSTRAP_POSITION_INDEPENDENT_CODE ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(vk_bootstrap VulkanMemoryAllocator glm fastgltf tinyobjloader stb)

# Exactly one renderer .cpp must #define STB_IMAGE_IMPLEMENTATION before including stb_image.h.
add_library(stb_image INTERFACE)
target_include_directories(stb_image SYSTEM INTERFACE ${stb_SOURCE_DIR})

# ---------------------------------------------------------------------------
# Shaders: nre_compile_shaders(<target> <shader_dir>), usable by the demo and
# by projects that fetch NRE.
# ---------------------------------------------------------------------------
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/NREShaders.cmake)

# ---------------------------------------------------------------------------
# Renderer: shared library (librenderer.so)
#
# The app links against the SONAME (librenderer.so.0.1 while at 0.1.x), so
# swapping renderers works between builds with the same SONAME. A breaking
# release changes it, and an old app then refuses to load the new .so instead
# of crashing. The app looks next to itself first ($ORIGIN), and
# LD_LIBRARY_PATH overrides that.
# ---------------------------------------------------------------------------
# Sources live in subsystem folders (core/, gpu/, pipeline/, scene/) and are
# included relative to src/renderer, e.g. #include "gpu/buffer.h".
file(GLOB_RECURSE RENDERER_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/*.h
)
if(NOT NRE_UNSAFE_API)
    list(FILTER RENDERER_SOURCES EXCLUDE REGEX "/src/renderer/unsafe/")
endif()

add_library(renderer SHARED ${RENDERER_SOURCES})
add_library(NRE::renderer ALIAS renderer)

# The public headers use C++23 (std::expected), so apps need at least that.
target_compile_features(renderer PUBLIC cxx_std_23)

# While 0.x every minor release may break the ABI, so it's part of the SONAME.
if(PROJECT_VERSION_MAJOR EQUAL 0)
    set(NRE_SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})
else()
    set(NRE_SOVERSION ${PROJECT_VERSION_MAJOR})
endif()

# Only symbols marked RENDERER_API are exported; vk-bootstrap, VMA and the
# renderer internals stay hidden.
set_target_properties(renderer PROPERTIES
    OUTPUT_NAME               renderer
    VERSION                   ${PROJECT_VERSION}
    SOVERSION                 ${NRE_SOVERSION}
    CXX_VISIBILITY_PRESET     hidden
    VISIBILITY_INLINES_HIDDEN ON
)

# Static libs linked into the .so (vk-bootstrap) are built with default
# visibility; keep their symbols out of the export table too.
target_link_options(renderer PRIVATE -Wl,--exclude-libs,ALL)

# Generates renderer_export.h with the RENDERER_API macro.
include(GenerateExportHeader)
generate_export_header(renderer
    BASE_NAME        renderer
    EXPORT_MACRO_NAME RENDERER_API
    EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/include/renderer_export.h
)

target_include_directories(renderer
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer
)

target_link_libraries(renderer
    PUBLIC
        glm::glm
    PRIVATE
        Vulkan::Vulkan
        vk-bootstrap::vk-bootstrap
        GPUOpen::VulkanMemoryAllocator
        fastgltf::fastgltf
        tinyobjloader
        stb_image
        glfw
)

# PUBLIC defines must match on both sides of the API, so the app gets them too.
target_compile_definitions(renderer
    PUBLIC
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_RADIANS
    PRIVATE
        GLFW_INCLUDE_VULKAN
        $<$<CONFIG:Debug>:RENDERER_ENABLE_VALIDATION>
)

if(NRE_UNSAFE_API)
    # PUBLIC so the app sees it too: renderer_unsafe.h refuses to compile without it.
    target_compile_definitions(renderer PUBLIC NRE_UNSAFE_API)
    # renderer_unsafe.h uses Vulkan types, so the app needs the Vulkan headers (not the loader library).
    target_link_libraries(renderer PUBLIC Vulkan::Headers)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(renderer PRIVATE -Wall -Wextra -Wpedantic -Werror=return-type)
endif()

# ---------------------------------------------------------------------------
# Demo app and benchmark: skipped automatically when another project fetches NRE.
# ---------------------------------------------------------------------------
if(PROJECT_IS_TOP_LEVEL)
    add_subdirectory(example)
    add_subdirectory(benchmark)
endif()
