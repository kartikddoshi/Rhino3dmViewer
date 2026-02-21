cmake_minimum_required(VERSION 3.10)
project(RhinoThumbs)

set(CMAKE_CXX_STANDARD 17)

# OpenNURBS
add_subdirectory(3rdparty/opennurbs)

# Core Library
add_subdirectory(src/core)

# Viewer Application
add_subdirectory(src/viewer)

# CLI Tool
add_subdirectory(src/cli)
