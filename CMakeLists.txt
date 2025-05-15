# Minimum CMake version required
cmake_minimum_required(VERSION 3.16)

# Project Name, Version, and Languages
project(IWEngine VERSION 0.1.0 LANGUAGES CXX) # Changed MyVulkanEngine to IWEngine

# Set C++ standard (e.g., C++17 or C++20)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
# Good practice to disable GNU extensions for better portability if not explicitly needed
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Output Directories (Optional, but organizes build output) ---
# This puts the executable in IWEngine/build/bin/
# and libraries (if any) in IWEngine/build/lib/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# --- Find Packages (Dependencies) ---

# SDL2 (Simple DirectMedia Layer - for windowing and input)
# CMake will use its FindSDL2.cmake module.
# Ensure SDL2 is installed (e.g., via 'brew install sdl2')
find_package(SDL2 REQUIRED)
message(STATUS "Found SDL2: ${SDL2_LIBRARIES}")
message(STATUS "SDL2 Include Dirs: ${SDL2_INCLUDE_DIRS}")
message(STATUS "SDL2 Version: ${SDL2_VERSION_STRING}")

# GLM (OpenGL Mathematics - for vector/matrix math, header-only)
# CMake will use its Findglm.cmake module.
# Ensure GLM is installed (e.g., via 'brew install glm')
find_package(glm REQUIRED)
message(STATUS "Found GLM, version: ${GLM_VERSION_STRING}") # GLM is header-only, primarily needs include dirs

# Vulkan (Graphics and Compute API)
# CMake will use its FindVulkan.cmake module.
# This relies on the VULKAN_SDK environment variable being set correctly
# to point to your installed Vulkan SDK.
find_package(Vulkan REQUIRED)
message(STATUS "Found Vulkan: ${Vulkan_LIBRARIES}")
message(STATUS "Vulkan Include Dirs: ${Vulkan_INCLUDE_DIRS}")
message(STATUS "Vulkan Version: ${Vulkan_VERSION_STRING}")

# --- Add Executable ---
# Defines the executable target and specifies its source files.
# THIS MUST COME BEFORE target_include_directories and target_link_libraries for this target.
add_executable(${PROJECT_NAME} # PROJECT_NAME will now be IWEngine
    src/main.cpp
    # Add other .cpp files here as your project grows
    # e.g., src/vulkan_renderer.cpp
    #       src/engine.cpp
)

# --- Include Directories ---
# Add directories where the compiler should look for header files.
# This allows you to use #include "my_header.h" if my_header.h is in the 'include' directory of your project,
# and #include <SDL.h>, #include <glm/glm.hpp>, #include <vulkan/vulkan.h> for the libraries.
target_include_directories(${PROJECT_NAME} PRIVATE # PROJECT_NAME will now be IWEngine
    "${CMAKE_CURRENT_SOURCE_DIR}/include" # Your project's custom include directory
    ${SDL2_INCLUDE_DIRS}                  # SDL2's include directories
    ${GLM_INCLUDE_DIRS}                   # GLM's include directories (often /usr/local/include or /opt/homebrew/include)
    ${Vulkan_INCLUDE_DIRS}                # Vulkan's include directories (from the SDK)
)

# --- Link Libraries ---
# Specifies the libraries that the executable target depends on.
target_link_libraries(${PROJECT_NAME} PRIVATE # PROJECT_NAME will now be IWEngine
    ${SDL2_LIBRARIES}    # Link against the SDL2 libraries found
    # GLM is typically header-only, so it doesn't usually require linking
    # unless you've built it as a static/shared library yourself.
    Vulkan::Vulkan       # This is the modern CMake imported target for Vulkan.
                         # It handles linking the Vulkan loader library.
)

# --- macOS Specific Settings ---
# These settings are for creating a proper application bundle on macOS.
if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES # PROJECT_NAME will now be IWEngine
        MACOSX_BUNDLE TRUE
        # MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist" # Optional: Path to your custom Info.plist file.
                                                                       # If not specified, CMake generates a basic one.
                                                                       # For SDL2 apps, SDL_main often handles some of this.
        # MACOSX_BUNDLE_ICON_FILE "YourIcon.icns" # Optional: Path to your app icon
    )

    # For Vulkan (MoltenVK) on macOS, the Vulkan loader needs to find MoltenVK.
    # This is typically handled by the VK_ICD_FILENAMES environment variable pointing
    # to the MoltenVK_icd.json file in your Vulkan SDK.
    # If you were creating a distributable bundle, you would need to:
    # 1. Copy the MoltenVK.framework (or libMoltenVK.dylib) into your app bundle's Frameworks directory.
    # 2. Set the RPATH for your executable to look inside @executable_path/../Frameworks.
    #    Example:
    #    set_target_properties(${PROJECT_NAME} PROPERTIES
    #        INSTALL_RPATH "@executable_path/../Frameworks"
    #        BUILD_WITH_INSTALL_RPATH TRUE
    #    )
    # For local development with the SDK installed and environment variables set,
    # explicit bundling of MoltenVK is usually not required.
endif()

# --- Final Status Messages ---
message(STATUS "CMake Project: ${PROJECT_NAME} configured successfully.") # PROJECT_NAME will now be IWEngine
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}") # Will show Debug, Release, etc.
message(STATUS "Executable will be placed in: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
