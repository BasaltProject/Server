# Decides whether the Bukkit compatibility layer is built, and embeds its runtime in the binary.

set(BASALT_BUILD_BUKKIT "AUTO" CACHE STRING "Bukkit layer: ON, OFF, or AUTO (built when a JDK and its jni.h are found)")
set(BASALT_BUKKIT_EMBED_DIR "${CMAKE_BINARY_DIR}/bukkit-embed" CACHE INTERNAL "Where the runtime pieces are staged before they are compiled in")
set(BASALT_SHIM_DIR "" CACHE PATH "A prebuilt Shim folder with libjvm and classpath.jar (empty builds the shim/ submodule)")

function(basalt_probe_bukkit RESULT)
    string(TOUPPER "${BASALT_BUILD_BUKKIT}" _mode)
    if(_mode STREQUAL "OFF" OR _mode STREQUAL "FALSE" OR _mode STREQUAL "0")
        set(${RESULT} OFF PARENT_SCOPE)
        return()
    endif()
    if(_mode STREQUAL "ON" OR _mode STREQUAL "TRUE" OR _mode STREQUAL "1")
        set(${RESULT} ON PARENT_SCOPE)
        return()
    endif()
    if(NOT (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin") OR CMAKE_CROSSCOMPILING)
        message(STATUS "Bukkit layer: off (only native Linux and macOS builds embed Shim)")
        set(${RESULT} OFF PARENT_SCOPE)
        return()
    endif()
    file(GLOB _jdks /usr/lib/jvm/*/include /Library/Java/JavaVirtualMachines/*/Contents/Home/include)
    find_path(_jni jni.h HINTS $ENV{JAVA_HOME}/include ${_jdks} NO_CACHE)
    find_program(_javac javac HINTS $ENV{JAVA_HOME}/bin NO_CACHE)
    find_program(_jar jar HINTS $ENV{JAVA_HOME}/bin NO_CACHE)
    if(NOT _jni OR NOT _javac OR NOT _jar)
        message(STATUS "Bukkit layer: off (no JDK with jni.h, javac and jar; install one or set JAVA_HOME to turn it on)")
        set(${RESULT} OFF PARENT_SCOPE)
        return()
    endif()
    message(STATUS "Bukkit layer: on (JDK found at ${_jni})")
    set(${RESULT} ON PARENT_SCOPE)
endfunction()

function(basalt_bukkit_runtime)
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/server-bukkit/scripts/build-bundle.sh")
        message(WARNING "Bukkit layer: server-bukkit/scripts/build-bundle.sh is missing, so no runtime folder is assembled")
        return()
    endif()
    set(_shim "${BASALT_SHIM_DIR}")
    set(_deps "")
    if(NOT _shim)
        if(EXISTS "${CMAKE_SOURCE_DIR}/shim/GNUmakefile")
            set(_shim "${CMAKE_BINARY_DIR}/shim")
            add_custom_target(basalt-shim ALL
                COMMAND ${CMAKE_SOURCE_DIR}/server-bukkit/scripts/build-shim.sh ${CMAKE_SOURCE_DIR}/shim ${_shim}
                        ${CMAKE_SOURCE_DIR}/wire/third_party/zlib
                COMMENT "Building Basalt Shim (the embedded Java machine)"
                VERBATIM
            )
            set(_deps DEPENDS basalt-shim)
        else()
            message(STATUS "Bukkit layer: shim/ is empty (git submodule update --init) and BASALT_SHIM_DIR is unset; no Shim in the runtime")
        endif()
    endif()
    add_custom_target(basalt-bukkit-runtime ALL
        COMMAND ${CMAKE_SOURCE_DIR}/server-bukkit/scripts/build-bundle.sh ${CMAKE_BINARY_DIR}/bukkit-work ${CMAKE_BINARY_DIR}/bukkit-runtime
                "${_shim}"
        ${_deps}
        COMMENT "Assembling the Bukkit runtime"
        VERBATIM
    )
    add_custom_target(basalt-bukkit-embed ALL
        COMMAND ${CMAKE_COMMAND} -DSHIM=${_shim} -DRUNTIME=${CMAKE_BINARY_DIR}/bukkit-runtime -DOUT=${BASALT_BUKKIT_EMBED_DIR}
                -P ${CMAKE_SOURCE_DIR}/cmake/StageBukkitRuntime.cmake
        COMMENT "Staging the Bukkit runtime to embed"
        VERBATIM
    )
    add_dependencies(basalt-bukkit-embed basalt-bukkit-runtime)
    add_dependencies(server-bukkit basalt-bukkit-embed)
endfunction()
