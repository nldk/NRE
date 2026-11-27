# Demo app. Only built when NRE is the top-level project (see the root CMakeLists.txt),
# so projects that fetch NRE don't get it.

add_executable(VulkanRenderer main.cpp)
target_link_libraries(VulkanRenderer PRIVATE NRE::renderer)

nre_compile_shaders(VulkanRenderer ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
target_compile_definitions(VulkanRenderer PRIVATE ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")

# librenderer.so sits next to the app in bin/.
set_target_properties(VulkanRenderer PROPERTIES
    BUILD_RPATH   "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(VulkanRenderer PRIVATE -Wall -Wextra -Wpedantic -Werror=return-type)
endif()

# The landscape demo (example/landscape).
add_subdirectory(landscape)
