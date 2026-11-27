# nre_compile_shaders(<target> <shader_dir> [<shader_dir>...])
#
# Compiles every .vert/.frag/.comp under each <shader_dir> to
# ${CMAKE_CURRENT_BINARY_DIR}/shaders/<name>.spv, builds them before <target>,
# and gives <target> a SHADER_DIR define pointing at that folder. NRE's shader includes
# (NRE_SHADER_INCLUDE_DIR) are on the include path, so shaders can
#     #include "nre/lighting.glsl"   (fragment shaders)  or  #include "nre/frame.glsl"   (vertex shaders)
#
# Available to projects that pull in NRE with FetchContent / add_subdirectory.

# Vulkan_GLSLC_EXECUTABLE is a cache variable, so it's visible from the caller's
# directory too (the Vulkan::glslc imported target isn't).
find_package(Vulkan 1.3 REQUIRED COMPONENTS glslc)

# A cache variable, so it's visible from the directory of a project that fetched NRE too.
cmake_path(SET _nre_shader_include_dir NORMALIZE "${CMAKE_CURRENT_LIST_DIR}/../shaders/include")
set(NRE_SHADER_INCLUDE_DIR "${_nre_shader_include_dir}" CACHE INTERNAL "NRE's GLSL include folder (nre/frame.glsl, nre/lighting.glsl)")
unset(_nre_shader_include_dir)

function(nre_compile_shaders target)
    set(out_dir ${CMAKE_CURRENT_BINARY_DIR}/shaders)

    set(sources)
    foreach(shader_dir ${ARGN})
        file(GLOB_RECURSE found CONFIGURE_DEPENDS
            ${shader_dir}/*.vert
            ${shader_dir}/*.frag
            ${shader_dir}/*.comp
        )
        list(APPEND sources ${found})
    endforeach()

    set(binaries)
    foreach(src ${sources})
        get_filename_component(name ${src} NAME)
        set(spv ${out_dir}/${name}.spv)
        add_custom_command(
            OUTPUT  ${spv}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${out_dir}
            COMMAND ${Vulkan_GLSLC_EXECUTABLE} --target-env=vulkan1.3 -O -g -I ${NRE_SHADER_INCLUDE_DIR} -MD -MF ${spv}.d -o ${spv} ${src}
            DEPENDS ${src}
            DEPFILE ${spv}.d
            COMMENT "Compiling shader ${name}"
            VERBATIM
        )
        list(APPEND binaries ${spv})
    endforeach()

    add_custom_target(${target}_shaders DEPENDS ${binaries})
    add_dependencies(${target} ${target}_shaders)
    target_compile_definitions(${target} PRIVATE SHADER_DIR="${out_dir}")
endfunction()
