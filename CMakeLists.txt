cmake_minimum_required(VERSION 3.28)

project(basalt-server
    VERSION 26.3.0
    DESCRIPTION "Basalt: a Minecraft: Java Edition server written from scratch"
    LANGUAGES C CXX
)

set(BASALT_VERSION "26.3.0-dev" CACHE STRING "Version stamped into the binary and its file name")

option(BASALT_BUILD_TESTS "Build the test executables" ON)
option(BASALT_BUILD_BENCH "Build the benchmark executables" OFF)
option(BASALT_WERROR "Treat warnings as errors" OFF)
option(BASALT_LINUX_MUSL "Build a Linux musl executable, fully static unless the Bukkit layer is on (requires a musl toolchain)" OFF)
if(ANDROID AND ANDROID_NATIVE_API_LEVEL LESS 29)
    message(FATAL_ERROR "Android requires API 29 or newer for the native asynchronous DNS resolver")
endif()
if(BASALT_LINUX_MUSL)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR "BASALT_LINUX_MUSL requires a Linux musl toolchain")
    endif()
    # A static musl executable cannot dlopen, and the Bukkit layer loads Shim that way; libc++ stays static either way
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/BasaltBukkit.cmake)
    basalt_probe_bukkit(BASALT_MUSL_BUKKIT)
    if(NOT BASALT_MUSL_BUKKIT)
        add_link_options(-static)
    endif()
endif()
set(BASALT_SANITIZE "" CACHE STRING "Comma-separated sanitizers: address, undefined, thread")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# The native sampler follows frame pointers; Linux compilers drop them by default, and macOS keeps them.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_compile_options(-fno-omit-frame-pointer)
endif()

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
include(BasaltBukkit)
basalt_probe_bukkit(BASALT_BUKKIT_ACTIVE)

add_subdirectory(metamorph)

add_subdirectory(server-random)
add_subdirectory(server-registry)
add_subdirectory(server-entity)
add_subdirectory(server-storage)
add_subdirectory(server-assets)
add_subdirectory(server-plugins)
add_subdirectory(server-world)
add_subdirectory(server-remote)
if(BASALT_BUKKIT_ACTIVE)
    add_subdirectory(server-bukkit)
endif()
add_subdirectory(server-runtime)
if(BASALT_BUKKIT_ACTIVE)
    basalt_bukkit_runtime()
endif()
