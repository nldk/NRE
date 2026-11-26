# CPU-only tests: no Vulkan device needed. Run with ctest.
add_executable(lightingMathTest lightingMathTest.cpp)
target_link_libraries(lightingMathTest PRIVATE renderer_headers)
target_include_directories(lightingMathTest PRIVATE ${CMAKE_SOURCE_DIR}/src/renderer)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lightingMathTest PRIVATE -Wall -Wextra -Wpedantic -Werror=return-type)
endif()
add_test(NAME lightingMath COMMAND lightingMathTest)
