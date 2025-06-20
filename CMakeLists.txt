cmake_minimum_required(VERSION 3.24)
project(T3Vengine LANGUAGES CXX)

# Use C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find SDL2 (via SDL2Config.cmake from Homebrew, vcpkg, etc.)
find_package(SDL2 REQUIRED)
find_package(GLEW REQUIRED)
find_package(glm REQUIRED)
find_package(OpenGL REQUIRED)

# Collect sources (recursively from src/ and maps/)
file(GLOB_RECURSE ENGINE_SOURCES
  ${CMAKE_SOURCE_DIR}/src/*.cpp
  ${CMAKE_SOURCE_DIR}/src/*.h
  ${CMAKE_SOURCE_DIR}/maps/*.cpp
  ${CMAKE_SOURCE_DIR}/maps/*.h
)

# Define executable
add_executable(T3Vengine ${ENGINE_SOURCES})

# Include directories
target_include_directories(T3Vengine PRIVATE
  ${CMAKE_SOURCE_DIR}/include   # for stb_image, tiny_obj_loader
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/maps
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

# Pass shader and asset dirs into code as defines
target_compile_definitions(T3Vengine PRIVATE
  SHADER_DIR="${CMAKE_SOURCE_DIR}/shader_sources"
  ASSET_DIR="${CMAKE_SOURCE_DIR}/assets"
)

# Copy the entire maps directory to the output directory (long-term best practice!)
add_custom_command(TARGET T3Vengine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/maps
        $<TARGET_FILE_DIR:T3Vengine>/maps
)

# If you want to copy assets/textures/shaders as well, add similar blocks:
 add_custom_command(TARGET T3Vengine POST_BUILD
     COMMAND ${CMAKE_COMMAND} -E copy_directory
         ${CMAKE_SOURCE_DIR}/assets
         $<TARGET_FILE_DIR:T3Vengine>/assets
 )
 add_custom_command(TARGET T3Vengine POST_BUILD
     COMMAND ${CMAKE_COMMAND} -E copy_directory
         ${CMAKE_SOURCE_DIR}/shader_sources
         $<TARGET_FILE_DIR:T3Vengine>/shader_sources
 )
