cmake_minimum_required(VERSION 3.28)
project(ShaderPlayGround LANGUAGES CXX)

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
if(NOT CMAKE_C_COMPILER)
    set(CMAKE_C_COMPILER $ENV{CC})
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(BUILD_SHARED_LIBS OFF)

# --- Dependencies ---
include(FetchContent)
set(FETCHCONTENT_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps_cache)
set(FETCHCONTENT_QUIET OFF)

# [cite_start]1. Знаходимо системний OpenGL [cite: 3]
find_package(OpenGL REQUIRED)

# 2. SDL3
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG main  
    GIT_SHALLOW TRUE
    FIND_PACKAGE_ARGS COMPONENTS SDL3-static 
)

message(STATUS "=== Fetching SDL3 ===")
FetchContent_MakeAvailable(SDL3)

message(STATUS "=== All dependencies fetched! ===")


# --- MAIN EXECUTABLE ---
add_executable(ShaderPlayGround
    src/main.cpp
)
target_include_directories(ShaderPlayGround
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders
    PRIVATE
        ${CMAKE_BINARY_DIR}
)

# [cite_start]3. Лінкуємо OpenGL::GL разом з SDL3 [cite: 3]
target_link_libraries(ShaderPlayGround
    PUBLIC
        SDL3::SDL3-static
        OpenGL::GL
)

set(SHADER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders")
set(SHADER_DEST_DIR "$<TARGET_FILE_DIR:ShaderPlayGround>/src/shaders")

add_custom_command(
    TARGET ShaderPlayGround POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_DEST_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${SHADER_SRC_DIR} ${SHADER_DEST_DIR}
    COMMENT "Copying shaders to build directory"
)

if(WIN32)
    set_property(TARGET ShaderPlayGround PROPERTY WIN32_EXECUTABLE TRUE)
endif()