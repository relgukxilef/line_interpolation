cmake_minimum_required(VERSION 3.5)

project(line_interpolation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(line_interpolation main.cpp)

add_subdirectory(game_engine1)

target_compile_definitions(line_interpolation PRIVATE GLEW_STATIC)

target_link_libraries(
    line_interpolation PRIVATE
    gdi32 user32 kernel32 glfw3 glew32s opengl32
    game_engine1_opengl
)

file(COPY shader/position_vertex.glsl DESTINATION shader)
