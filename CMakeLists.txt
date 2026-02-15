cmake_minimum_required(VERSION 3.15)
project(cimple LANGUAGES CXX)

# Minimal top-level CMake - expand as needed
set(CMAKE_CXX_STANDARD 17)
add_subdirectory(runtime_lib)
add_subdirectory(tools/cimple_cli)
