cmake_minimum_required(VERSION 3.24)
project(T3Vengine LANGUAGES CXX)

# Use C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find SDL2 (Homebrew’s SDL2Config.cmake should be available)
find_package(SDL2 REQUIRED)

# Find GLEW, GLM, OpenGL
find_package(GLEW REQUIRED)
find_package(glm REQUIRED)
find_package(OpenGL REQUIRED)

# Collect sources
file(GLOB_RECURSE ENGINE_SOURCES
  ${CMAKE_SOURCE_DIR}/src/*.cpp
  ${CMAKE_SOURCE_DIR}/src/*.h
)

# Define executable
add_executable(T3Vengine ${ENGINE_SOURCES})

# Include directories
target_include_directories(T3Vengine PRIVATE
  ${CMAKE_SOURCE_DIR}/include   # stb_image, tiny_obj_loader
  ${CMAKE_SOURCE_DIR}/src
  ${SDL2_INCLUDE_DIRS}
  ${GLEW_INCLUDE_DIRS}
  ${GLM_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(T3Vengine PRIVATE
  SDL2::SDL2          # core SDL2
  SDL2::SDL2main      # SDL2 main shim
  GLEW::GLEW          # GLEW loader
  glm::glm            # GLM math
  OpenGL::GL          # OpenGL
)

# Pass shader and asset dirs into code
target_compile_definitions(T3Vengine PRIVATE
  SHADER_DIR="${CMAKE_SOURCE_DIR}/shader_sources"
  ASSET_DIR="${CMAKE_SOURCE_DIR}/assets"
)

# On macOS you might need frameworks for SDL2; these are automatically
# added by SDL2::SDL2, but if you still see missing symbols, uncomment:
# find_library(COCOA Cocoa)
# target_link_libraries(T3Vengine PRIVATE ${COCOA})
