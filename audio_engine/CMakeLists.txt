cmake_minimum_required(VERSION 3.16)
project(dendrophone_audio_engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags for real-time audio
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -ffast-math")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

# Find ALSA
find_package(ALSA REQUIRED)

# Core library (header-only for now)
add_library(dendrophone_core INTERFACE)
target_include_directories(dendrophone_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/core
)

# IO library
add_library(dendrophone_io STATIC
    io/alsa_device.cpp
)
target_include_directories(dendrophone_io PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(dendrophone_io PUBLIC
    ALSA::ALSA
    dendrophone_core
)

# Engine library
add_library(dendrophone_engine STATIC
    engine/audio_engine.cpp
)
target_include_directories(dendrophone_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(dendrophone_engine PUBLIC
    dendrophone_io
    dendrophone_core
    pthread
)

# Main executable
add_executable(dendrophone main.cpp)
target_link_libraries(dendrophone PRIVATE
    dendrophone_engine
)

# Install
install(TARGETS dendrophone RUNTIME DESTINATION bin)
