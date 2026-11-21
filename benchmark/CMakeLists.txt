# SceneMode vs FunctionDrawMode benchmark. Only built when NRE is the top-level project.
# Run a Release build: Debug adds validation layers and per-call checks.

add_executable(nre_benchmark main.cpp)
target_link_libraries(nre_benchmark PRIVATE NRE::renderer)

# Uses the demo's shaders and Box.glb.
nre_compile_shaders(nre_benchmark ${PROJECT_SOURCE_DIR}/example/shaders)
target_compile_definitions(nre_benchmark PRIVATE ASSET_DIR="${PROJECT_SOURCE_DIR}/example/assets")

set_target_properties(nre_benchmark PROPERTIES
    BUILD_RPATH   "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(nre_benchmark PRIVATE -Wall -Wextra -Wpedantic -Werror=return-type)
endif()
