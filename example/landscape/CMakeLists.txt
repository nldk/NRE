# Landscape demo: a world built in code under the Landscape lighting mode. Uses the example's shaders.
add_executable(nre_landscape main.cpp world.cpp world.h)
target_link_libraries(nre_landscape PRIVATE NRE::renderer)

nre_compile_shaders(nre_landscape ${CMAKE_CURRENT_SOURCE_DIR}/../shaders)
target_compile_definitions(nre_landscape PRIVATE LANDSCAPE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

set_target_properties(nre_landscape PROPERTIES
    BUILD_RPATH   "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(nre_landscape PRIVATE -Wall -Wextra -Wpedantic -Werror=return-type)
endif()
