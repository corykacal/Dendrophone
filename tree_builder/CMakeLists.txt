cmake_minimum_required(VERSION 3.21)
project(tree_builder VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Quick)

# DPT Parser library sources
set(DPT_SOURCES
    src/dendrophone/graph/dpt_parser.cpp
    src/dendrophone/graph/graph_normalizer.cpp
    src/dendrophone/graph/graph_compiler.cpp
    src/dendrophone/effects/delay.cpp
    src/dendrophone/effects/lfo.cpp
    src/dendrophone/effects/envelope.cpp
    src/dendrophone/effects/reverb.cpp
    src/dendrophone/effects/pitch_shift.cpp
    src/dendrophone/effects/svfilter.cpp
    src/dendrophone/effects/reverse.cpp
    src/dendrophone/effects/multitap_delay.cpp
    src/dendrophone/effects/granular.cpp
    src/dendrophone/effects/looper.cpp
    src/dendrophone/effects/glitch.cpp
    src/dendrophone/effects/chorus.cpp
    src/dendrophone/effects/tremolo.cpp
    src/dendrophone/effects/flanger.cpp
    src/dendrophone/effects/compressor.cpp
    src/dendrophone/effects/overdrive.cpp
    src/dendrophone/effects/autowah.cpp
)

# Application sources
set(APP_SOURCES
    src/main.cpp
    src/dpt_model.cpp
    src/node_model.cpp
)

# Create executable
qt_add_executable(tree_builder
    ${APP_SOURCES}
    ${DPT_SOURCES}
)

# Add QML module
qt_add_qml_module(tree_builder
    URI TreeBuilder
    VERSION 1.0
    QML_FILES
        qml/main.qml
        qml/GraphCanvas.qml
        qml/NodeItem.qml
        qml/EdgeItem.qml
)

target_include_directories(tree_builder PRIVATE
    src
    src/dendrophone
    src/dendrophone/external
)

target_link_libraries(tree_builder PRIVATE
    Qt6::Core
    Qt6::Quick
)
