# Minimum CMake version required
cmake_minimum_required(VERSION 3.16)

# Project Name, Version, and Languages
project(IWEngine VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard globally (good practice, but we'll also set it per target)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF) # Disable GNU extensions for better portability

# For std::filesystem with some compilers/linkers, especially older ones or on specific platforms,
# you might need to explicitly link to a filesystem library.
# With modern AppleClang and C++17, this is usually not needed if the standard is set correctly.
# However, if issues persist, you might try finding and linking Threads (often a dependency for filesystem).
# find_package(Threads REQUIRED)

# --- Output Directories (Optional, but organizes build output) ---
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# --- Find Packages (Dependencies) ---
find_package(SDL2 REQUIRED)
message(STATUS "Found SDL2: ${SDL2_LIBRARIES}")
message(STATUS "SDL2 Include Dirs: ${SDL2_INCLUDE_DIRS}")
message(STATUS "SDL2 Version: ${SDL2_VERSION_STRING}")

find_package(glm REQUIRED)
message(STATUS "Found GLM, version: ${GLM_VERSION_STRING}")

find_package(Vulkan REQUIRED)
message(STATUS "Found Vulkan: ${Vulkan_LIBRARIES}")
message(STATUS "Vulkan Include Dirs: ${Vulkan_INCLUDE_DIRS}")
message(STATUS "Vulkan Version: ${Vulkan_VERSION_STRING}")

# --- Add Executable ---
add_executable(${PROJECT_NAME}
    src/main.cpp
)

# --- Set C++ Standard and Features for the Target ---
# This is a more explicit way to ensure C++17 features are enabled for your specific target.
# For C++17, 'cxx_std_17' is the feature name.
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

# Alternatively, or in addition if problems persist, you could directly add compiler flags:
# if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
#     target_compile_options(${PROJECT_NAME} PRIVATE -std=c++17)
# elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
#     target_compile_options(${PROJECT_NAME} PRIVATE -std=c++17)
# elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
#     target_compile_options(${PROJECT_NAME} PRIVATE /std:c++17)
# endif()


# --- Include Directories ---
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include" 
    ${SDL2_INCLUDE_DIRS}                  
    ${GLM_INCLUDE_DIRS}                   
    ${Vulkan_INCLUDE_DIRS}                
)

# --- Link Libraries ---
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${SDL2_LIBRARIES}    
    Vulkan::Vulkan       
    # For std::filesystem, if your linker complains on some platforms (less common with modern AppleClang):
    # if(CMAKE_VERSION VERSION_LESS "3.17") # Older CMake might need more help
    #    if(NOT APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    #        target_link_libraries(${PROJECT_NAME} PRIVATEstdc++fs)
    #    endif()
    # endif()
    # If using Threads explicitly:
    # Threads::Threads
)

# --- macOS Specific Settings ---
if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
    )
    # If you find you need to explicitly link against libc++ for filesystem on macOS
    # (which should be rare with modern Xcode/Clang if C++17 is active):
    # target_link_options(${PROJECT_NAME} PRIVATE "-lc++fs") # This is usually not needed
                                                          # or even "-lc++" if basic std lib symbols are missing
endif()

# --- Final Status Messages ---
message(STATUS "CMake Project: ${PROJECT_NAME} configured successfully.")
message(STATUS "C++ Standard specified for target: 17")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}") 
message(STATUS "Executable will be placed in: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
