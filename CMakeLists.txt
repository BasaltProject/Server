cmake_minimum_required(VERSION 3.28)

project(basalt-server
    VERSION 26.2.0
    DESCRIPTION "Basalt: a Minecraft: Java Edition server written from scratch"
    LANGUAGES C CXX
)

set(BASALT_VERSION "26.2.0-dev" CACHE STRING "Version stamped into the binary and its file name")

option(BASALT_BUILD_TESTS "Build the test executables" ON)
option(BASALT_BUILD_BENCH "Build the benchmark executables" OFF)
option(BASALT_WERROR "Treat warnings as errors" OFF)
set(BASALT_SANITIZE "" CACHE STRING "Comma-separated sanitizers: address, undefined, thread")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# No RTTI: nothing in the server needs it and it grows the binary.
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/wire/CMakeLists.txt)
    message(FATAL_ERROR "wire/ is empty. Run: git submodule update --init")
endif()

if(BASALT_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(wire)

include(BasaltBinary)

add_subdirectory(server-random)
add_subdirectory(server-world)
add_subdirectory(server-runtime)
