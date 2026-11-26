# CPU-only tests: no Vulkan device needed. Run with ctest.
add_executable(lightingMathTest lightingMathTest.cpp)
target_link_libraries(lightingMathTest PRIVATE renderer_headers)
target_include_directories(lightingMathTest PRIVATE ${CMAKE_SOURCE_DIR}/src/renderer)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lightingMathTest PRIVATE -Wall -Wextra -Wpedantic -Werror=return-type)
endif()
add_test(NAME lightingMath COMMAND lightingMathTest)

# GPU tests: render offscreen through the unsafe render target and read it back. A window opens while they run.
if(NRE_UNSAFE_API)
    add_executable(nre_visual_test visual/visualTest.cpp visual/capture.cpp)
    target_link_libraries(nre_visual_test PRIVATE NRE::renderer Vulkan::Vulkan stb_image)
    target_include_directories(nre_visual_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/visual)
    nre_compile_shaders(nre_visual_test ${PROJECT_SOURCE_DIR}/example/shaders ${CMAKE_CURRENT_SOURCE_DIR}/visual/shaders)
    target_compile_definitions(nre_visual_test PRIVATE
        ASSET_DIR="${PROJECT_SOURCE_DIR}/example/assets"
        TEST_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
        GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/visual/golden"
    )
    set_target_properties(nre_visual_test PROPERTIES
        BUILD_RPATH   "$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
    )
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(nre_visual_test PRIVATE -Wall -Wextra -Wpedantic -Werror=return-type -Wno-missing-field-initializers)
    endif()

    # One test per scenario name registered with NRE_SCENARIO (underscores become dashes).
    foreach(scenario capture-clear baseline-car baseline-skybox far-object
                     exposure-brighter sky-color function-mode-car skybox-exposure resize-target passthrough-2d
                     alpha-mask double-sided roughness no-uv-mesh demo-car-lit normal-map metallic-texture
                     shadow-basic shadow-function-mode shadow-off shadow-no-cast shadow-alpha shadow-noon
                     shadow-far shadow-stable shadow-cascade-borders scene-lighting-defaults
                     sky-noon sky-sunset sky-night sun-disk sky-follows-sun sky-elevations lit-low-sun
                     scene-switch skybox-landscape)
        add_test(NAME ${scenario} COMMAND nre_visual_test ${scenario})
        set_tests_properties(${scenario} PROPERTIES
            # vk-bootstrap's debug messenger prints "[<severity>: <types>]", e.g. "[ERROR: Validation]" or
            # "[WARNING: General | Validation]"; any validation error or warning fails the test.
            FAIL_REGULAR_EXPRESSION "Validation Error;\\[(ERROR|WARNING): [^]]*Validation"
            ENVIRONMENT "NRE_TEST_OUT=${CMAKE_CURRENT_BINARY_DIR}/out"
        )
    endforeach()
endif()
