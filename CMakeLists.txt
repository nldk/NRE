cmake_minimum_required(VERSION 3.28)
project(VulkanRenderer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# System dependencies
# ---------------------------------------------------------------------------
find_package(Vulkan 1.3 REQUIRED COMPONENTS glslc)
find_package(glfw3 3.4 REQUIRED)

# ---------------------------------------------------------------------------
# Fetched dependencies
# ---------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
    vk_bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
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
FetchContent_MakeAvailable(vk_bootstrap VulkanMemoryAllocator glm fastgltf tinyobjloader)

# ---------------------------------------------------------------------------
# Shaders: every .vert/.frag/.comp in shaders/ is compiled to build/shaders/*.spv
# ---------------------------------------------------------------------------
set(SHADER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)

file(GLOB_RECURSE SHADER_SOURCES CONFIGURE_DEPENDS
    ${SHADER_SRC_DIR}/*.vert
    ${SHADER_SRC_DIR}/*.frag
    ${SHADER_SRC_DIR}/*.comp
)

set(SHADER_BINARIES)
foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SPV ${SHADER_OUT_DIR}/${SHADER_NAME}.spv)
    add_custom_command(
        OUTPUT  ${SPV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUT_DIR}
        COMMAND Vulkan::glslc --target-env=vulkan1.3 -O -g -MD -MF ${SPV}.d -o ${SPV} ${SHADER}
        DEPENDS ${SHADER}
        DEPFILE ${SPV}.d
        COMMENT "Compiling shader ${SHADER_NAME}"
        VERBATIM
    )
    list(APPEND SHADER_BINARIES ${SPV})
endforeach()

add_custom_target(shaders DEPENDS ${SHADER_BINARIES})

# ---------------------------------------------------------------------------
# Renderer: shared library (librenderer.so)
#
# Swap renderers by replacing bin/librenderer.so with another build of the same
# API header. The app looks next to itself first ($ORIGIN), and
# LD_LIBRARY_PATH overrides that.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE RENDERER_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/*.cpp
)

add_library(renderer SHARED ${RENDERER_SOURCES}
        src/renderer/renderer.cpp
        src/renderer/context.h
        src/renderer/errorHandeling.cpp
        src/renderer/errorHandeling.h
        src/renderer/shader.cpp
        src/renderer/shader.h
        src/renderer/resourceManagement.cpp
        src/renderer/resourceManagement.h
        src/renderer/pipeline.cpp
        src/renderer/pipeline.h
        src/renderer/model.cpp
        src/renderer/model.h
        src/renderer/buffer.cpp
        src/renderer/buffer.h
        src/renderer/upload.cpp
        src/renderer/upload.h
        src/renderer/scene.cpp
        src/renderer/scene.h)
add_dependencies(renderer shaders)

# Only symbols marked RENDERER_API are exported; vk-bootstrap, VMA and the
# renderer internals stay hidden.
set_target_properties(renderer PROPERTIES
    OUTPUT_NAME               renderer
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
        glfw
)

# PUBLIC defines must match on both sides of the API, so the app gets them too.
target_compile_definitions(renderer
    PUBLIC
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_RADIANS
        SHADER_DIR="${SHADER_OUT_DIR}"
        ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
    PRIVATE
        GLFW_INCLUDE_VULKAN
        $<$<CONFIG:Debug>:RENDERER_ENABLE_VALIDATION>
)

# ---------------------------------------------------------------------------
# App
# ---------------------------------------------------------------------------
add_executable(VulkanRenderer src/main.cpp)

target_link_libraries(VulkanRenderer PRIVATE renderer)

set_target_properties(VulkanRenderer PROPERTIES
    BUILD_RPATH   "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
)

# ---------------------------------------------------------------------------
# Common settings
# ---------------------------------------------------------------------------
foreach(TARGET renderer VulkanRenderer)
    set_target_properties(${TARGET} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${TARGET} PRIVATE -Wall -Wextra -Wpedantic -Werror=return-type)
    endif()
endforeach()
