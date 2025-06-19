cmake_minimum_required(VERSION 3.24)

# Export compile_commands.json for IDEs
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(T3Vengine LANGUAGES CXX)

# Use C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# SDL2 via pkg-config (macOS Homebrew or system)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
include_directories(${SDL2_INCLUDE_DIRS})
link_directories(${SDL2_LIBRARY_DIRS})

# GLEW for OpenGL function loading
find_package(GLEW REQUIRED)

# GLM for mathematics
find_package(glm REQUIRED)

# Gather all engine source files
file(GLOB_RECURSE ENGINE_SOURCES
  ${CMAKE_SOURCE_DIR}/src/*.cpp
  ${CMAKE_SOURCE_DIR}/src/*.h
)

# Define the executable target
add_executable(T3Vengine ${ENGINE_SOURCES})
target_compile_features(T3Vengine PUBLIC cxx_std_23)

# Include directories
target_include_directories(T3Vengine PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/src
  ${SDL2_INCLUDE_DIRS}
  ${GLEW_INCLUDE_DIRS}
  ${GLM_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(T3Vengine PRIVATE
  ${SDL2_LIBRARIES}   # SDL2 headers & libraries
  GLEW::GLEW          # GLEW OpenGL loader
  glm::glm            # GLM mathematics
)

# Platform-specific OpenGL linkage
if(APPLE)
  find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
  target_link_libraries(T3Vengine PRIVATE
    ${COCOA_FRAMEWORK}
    "-framework OpenGL"
  )
elseif(WIN32)
  target_link_libraries(T3Vengine PRIVATE opengl32)
else()
  find_package(OpenGL REQUIRED)
  target_link_libraries(T3Vengine PRIVATE OpenGL::GL)
endif()

# Pass shader directory into the code
target_compile_definitions(T3Vengine PRIVATE
  SHADER_DIR="${CMAKE_SOURCE_DIR}/shader_sources"
)
