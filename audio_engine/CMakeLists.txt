cmake_minimum_required(VERSION 3.16)
project(dendrophone_audio_engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags for real-time audio
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -ffast-math")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

# Platform-specific audio library
if(APPLE)
    find_path(PORTAUDIO_INCLUDE_DIR portaudio.h
        HINTS /opt/homebrew/include /usr/local/include
    )
    find_library(PORTAUDIO_LIBRARY portaudio
        HINTS /opt/homebrew/lib /usr/local/lib
    )
    if(NOT PORTAUDIO_INCLUDE_DIR OR NOT PORTAUDIO_LIBRARY)
        message(FATAL_ERROR "PortAudio not found. Install with: brew install portaudio")
    endif()
    set(AUDIO_IO_SOURCES      io/portaudio_device.cpp)
    set(AUDIO_IO_INCLUDE_DIRS ${PORTAUDIO_INCLUDE_DIR})
    set(AUDIO_IO_LIBRARIES    ${PORTAUDIO_LIBRARY})
    set(AUDIO_IO_LINK_OPTIONS "")
    add_compile_definitions(USE_PORTAUDIO)
    message(STATUS "macOS: using PortAudio (${PORTAUDIO_LIBRARY})")
else()
    find_package(ALSA REQUIRED)
    set(AUDIO_IO_SOURCES      io/alsa_device.cpp)
    set(AUDIO_IO_INCLUDE_DIRS "")
    set(AUDIO_IO_LIBRARIES    ALSA::ALSA)
    set(AUDIO_IO_LINK_OPTIONS "")
    add_compile_definitions(USE_ALSA)
    message(STATUS "Linux: using ALSA")
endif()

# Core library (header-only for now)
add_library(dendrophone_core INTERFACE)
target_include_directories(dendrophone_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/core
)

# IO library
add_library(dendrophone_io STATIC ${AUDIO_IO_SOURCES})
target_include_directories(dendrophone_io PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${AUDIO_IO_INCLUDE_DIRS}
)
target_link_libraries(dendrophone_io PUBLIC
    ${AUDIO_IO_LIBRARIES}
    dendrophone_core
)
if(AUDIO_IO_LINK_OPTIONS)
    target_link_options(dendrophone_io PUBLIC ${AUDIO_IO_LINK_OPTIONS})
endif()

# Effects library
add_library(dendrophone_effects STATIC
    effects/autowah.cpp
    effects/chorus.cpp
    effects/compressor.cpp
    effects/delay.cpp
    effects/envelope.cpp
    effects/flanger.cpp
    effects/gain.cpp
    effects/glitch.cpp
    effects/granular.cpp
    effects/lfo.cpp
    effects/looper.cpp
    effects/multitap_delay.cpp
    effects/crunchy.cpp
    effects/pitch_shift.cpp
    effects/reverb.cpp
    effects/reverse.cpp
    effects/svfilter.cpp
)
target_include_directories(dendrophone_effects PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(dendrophone_effects PUBLIC
    dendrophone_core
)

# nlohmann/json (used by graph parser)
find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp
    HINTS /opt/homebrew/include /usr/local/include /usr/include
)
if(NOT NLOHMANN_JSON_INCLUDE_DIR)
    message(FATAL_ERROR "nlohmann/json not found. Install with: brew install nlohmann-json")
endif()
message(STATUS "nlohmann/json: ${NLOHMANN_JSON_INCLUDE_DIR}")

# Graph library
add_library(dendrophone_graph STATIC
    graph/dpt_parser.cpp
    graph/graph_normalizer.cpp
    graph/graph_compiler.cpp
)
target_include_directories(dendrophone_graph PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${NLOHMANN_JSON_INCLUDE_DIR}
)
target_link_libraries(dendrophone_graph PUBLIC
    dendrophone_core
    dendrophone_effects
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
    dendrophone_graph
    pthread
)

# Main executable
add_executable(dendrophone main.cpp)
target_link_libraries(dendrophone PRIVATE
    dendrophone_engine
)

# Install
install(TARGETS dendrophone RUNTIME DESTINATION bin)
