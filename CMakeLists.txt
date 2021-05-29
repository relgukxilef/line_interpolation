cmake_minimum_required(VERSION 3.5)

project(line_interpolation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(
    line_interpolation main.cpp
)

add_subdirectory(game_engine1)

target_compile_definitions(line_interpolation PRIVATE GLEW_STATIC)

target_link_libraries(
    line_interpolation PRIVATE
    gdi32 user32 kernel32 glfw3 glew32s opengl32 -static
    game_engine1_opengl
)

function(add_file TARGET DIRECTORY)
    foreach(file ${ARGN})
        get_filename_component(filename ${file} NAME)
        set(output ${CMAKE_CURRENT_BINARY_DIR}/${DIRECTORY}/${filename})
        target_sources(
            ${TARGET} PRIVATE
            ${output} ${file}
        )
        set_source_files_properties(
            ${output} PROPERTIES GENERATED TRUE
        )
        configure_file(${file} ${output} COPYONLY)
    endforeach()
endfunction()

add_file(
    line_interpolation shader
    shader/position_vertex.glsl
    shader/lines_fragment.glsl
)
