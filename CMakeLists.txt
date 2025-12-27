cmake_minimum_required(VERSION 3.28)
project(VoxelGame LANGUAGES CXX)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

# --- C++20 Modules Support ---
set(CMAKE_EXPERIMENTAL_CXX_MODULE_CMAKE_API "2182bf5c-ef0d-489a-91da-49dbc3090d2a")
set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP ON)

# --- Compiler Configuration ---
if(NOT CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER $ENV{CXX})
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(BUILD_SHARED_LIBS OFF)

# --- Dependencies ---
include(FetchContent)
find_package(OpenGL REQUIRED)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG main  
    GIT_SHALLOW TRUE
    FIND_PACKAGE_ARGS COMPONENTS SDL3-static 
)
FetchContent_MakeAvailable(SDL3)

# --- Main Executable ---
add_executable(VoxelGame
    src/main.cpp
)

# --- Modules ---
target_sources(VoxelGame
    PUBLIC
        FILE_SET CXX_MODULES FILES
            src/modules/Types.cppm
            src/modules/Math.cppm
            src/modules/GL.cppm
            src/modules/Shader.cppm
            src/modules/World.cppm
            src/modules/Meshing.cppm
)

target_include_directories(VoxelGame
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders
)

target_link_libraries(VoxelGame
    PUBLIC
        SDL3::SDL3-static
        OpenGL::GL
)

# Copy shaders
set(SHADER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders")
set(SHADER_DEST_DIR "$<TARGET_FILE_DIR:VoxelGame>/src/shaders")
add_custom_command(
    TARGET VoxelGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_DEST_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${SHADER_SRC_DIR} ${SHADER_DEST_DIR}
    COMMENT "Copying shaders to build directory"
)