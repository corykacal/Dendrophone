cmake_minimum_required(VERSION 3.10)
project(dendrophone)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(DRM REQUIRED libdrm)

add_executable(program
        src/main.cpp
)

target_link_libraries(program
        stdc++
        gcc
        EGL
        GLESv2
        gbm
        drm
        gpiod
        gpiodcxx
)

# Add DRM include directories
target_include_directories(program PRIVATE ${DRM_INCLUDE_DIRS})