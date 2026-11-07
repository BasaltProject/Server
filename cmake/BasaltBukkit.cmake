# Decides whether the Bukkit compatibility layer is built, and embeds its runtime in the binary.

set(BASALT_BUILD_BUKKIT "AUTO" CACHE STRING "Bukkit layer: ON, OFF, or AUTO (built when a JDK and its jni.h are found)")
set(BASALT_BUKKIT_EMBED_DIR "${CMAKE_BINARY_DIR}/bukkit-embed" CACHE INTERNAL "Where the runtime pieces are staged before they are compiled in")
set(BASALT_SHIM_DIR "" CACHE PATH "A prebuilt Shim folder with the VM library and classpath.jar (empty builds the shim/ submodule)")

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
    file(GLOB _jdks /usr/lib/jvm/*/include /usr/local/openjdk*/include /Library/Java/JavaVirtualMachines/*/Contents/Home/include)
    # The JDK belongs to the build machine, so a cross build's sysroot must not hide it
    find_path(_jni jni.h HINTS $ENV{JAVA_HOME}/include ${_jdks} NO_CACHE NO_CMAKE_FIND_ROOT_PATH)
    find_program(_javac javac HINTS $ENV{JAVA_HOME}/bin NO_CACHE NO_CMAKE_FIND_ROOT_PATH)
    find_program(_jar jar HINTS $ENV{JAVA_HOME}/bin NO_CACHE NO_CMAKE_FIND_ROOT_PATH)
    if(NOT _jni OR NOT _javac OR NOT _jar)
        message(STATUS "Bukkit layer: off (no JDK with jni.h, javac and jar; install one or set JAVA_HOME to turn it on)")
        set(${RESULT} OFF PARENT_SCOPE)
        return()
    endif()
    message(STATUS "Bukkit layer: on (JDK found at ${_jni})")
    set(${RESULT} ON PARENT_SCOPE)
endfunction()

# Windows cannot run a .sh by itself, and the bash on PATH may be the WSL stub, so prefer Git's
set(BASALT_BASH "")
if(WIN32)
    find_program(BASALT_BASH_EXE bash HINTS "$ENV{ProgramFiles}/Git/bin" "C:/Program Files/Git/bin" NO_DEFAULT_PATH)
    if(NOT BASALT_BASH_EXE)
        find_program(BASALT_BASH_EXE bash)
    endif()
    set(BASALT_BASH "${BASALT_BASH_EXE}")
endif()

function(basalt_bukkit_runtime)
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/server-bukkit/scripts/build-bundle.sh")
        message(WARNING "Bukkit layer: server-bukkit/scripts/build-bundle.sh is missing, so no runtime folder is assembled")
        return()
    endif()
    set(_shim "${BASALT_SHIM_DIR}")
    set(_deps "")
    if(NOT _shim)
        if(EXISTS "${CMAKE_SOURCE_DIR}/shim/CMakeLists.txt")
            set(_shim "${CMAKE_BINARY_DIR}/shim")
            # A cross build hands Shim the same toolchain it is using itself
            set(_cross_file "")
            if(CMAKE_CROSSCOMPILING)
                set(_cross_file "${CMAKE_BINARY_DIR}/shim-cross-args.txt")
                set(_cross_args "")
                foreach(_name CMAKE_TOOLCHAIN_FILE ANDROID_ABI ANDROID_PLATFORM ANDROID_STL CMAKE_SYSTEM_NAME
                              CMAKE_SYSTEM_PROCESSOR CMAKE_C_COMPILER CMAKE_CXX_COMPILER CMAKE_ASM_COMPILER
                              CMAKE_C_COMPILER_TARGET CMAKE_CXX_COMPILER_TARGET CMAKE_ASM_COMPILER_TARGET
                              CMAKE_SYSROOT CMAKE_CXX_FLAGS CMAKE_EXE_LINKER_FLAGS CMAKE_SHARED_LINKER_FLAGS
                              CMAKE_CXX_STANDARD_LIBRARIES CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
                              CMAKE_FIND_ROOT_PATH_MODE_PROGRAM CMAKE_FIND_ROOT_PATH_MODE_LIBRARY
                              CMAKE_FIND_ROOT_PATH_MODE_INCLUDE CMAKE_FIND_ROOT_PATH_MODE_PACKAGE)
                    if(DEFINED ${_name} AND NOT "${${_name}}" STREQUAL "")
                        string(APPEND _cross_args "-D${_name}=${${_name}}\n")
                    endif()
                endforeach()
                # Only the NDK brings a strip that understands the target; the host's would reject the library
                if(ANDROID)
                    string(APPEND _cross_args "-DCMAKE_STRIP=${CMAKE_STRIP}\n")
                else()
                    string(APPEND _cross_args "-DCMAKE_STRIP=OFF\n")
                endif()
                # The VM is a shared library, so it takes the executable's linker choice (such as lld) as well
                if(NOT CMAKE_SHARED_LINKER_FLAGS AND CMAKE_EXE_LINKER_FLAGS)
                    string(APPEND _cross_args "-DCMAKE_SHARED_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}\n")
                endif()
                file(WRITE "${_cross_file}" "${_cross_args}")
            endif()
            # Shim builds with the compilers this build uses, except on Windows where it needs MinGW instead
            set(_shim_env "")
            if(NOT WIN32 AND NOT CMAKE_CROSSCOMPILING)
                set(_shim_env ${CMAKE_COMMAND} -E env "CC=${CMAKE_C_COMPILER}" "CXX=${CMAKE_CXX_COMPILER}")
            endif()
            add_custom_target(basalt-shim ALL
                COMMAND ${_shim_env} ${BASALT_BASH} ${CMAKE_SOURCE_DIR}/server-bukkit/scripts/build-shim.sh ${CMAKE_SOURCE_DIR}/shim ${_shim}
                        ${CMAKE_SOURCE_DIR}/wire/third_party/zlib ${_cross_file}
                COMMENT "Building Basalt Shim (the embedded Java machine)"
                VERBATIM
            )
            set(_deps DEPENDS basalt-shim)
        else()
            message(STATUS "Bukkit layer: shim/ is empty (git submodule update --init) and BASALT_SHIM_DIR is unset; no Shim in the runtime")
        endif()
    endif()
    add_custom_target(basalt-bukkit-runtime ALL
        COMMAND ${BASALT_BASH} ${CMAKE_SOURCE_DIR}/server-bukkit/scripts/build-bundle.sh ${CMAKE_BINARY_DIR}/bukkit-work ${CMAKE_BINARY_DIR}/bukkit-runtime
                "${_shim}"
        ${_deps}
        COMMENT "Assembling the Bukkit runtime"
        VERBATIM
    )
    add_custom_target(basalt-bukkit-embed ALL
        BYPRODUCTS ${BASALT_BUKKIT_EMBED_DIR}/libjvm ${BASALT_BUKKIT_EMBED_DIR}/boot.jar
                   ${BASALT_BUKKIT_EMBED_DIR}/runtime.jar ${BASALT_BUKKIT_EMBED_DIR}/libs.txt
        COMMAND ${CMAKE_COMMAND} -DSHIM=${_shim} -DRUNTIME=${CMAKE_BINARY_DIR}/bukkit-runtime -DOUT=${BASALT_BUKKIT_EMBED_DIR}
                -P ${CMAKE_SOURCE_DIR}/cmake/StageBukkitRuntime.cmake
        COMMENT "Staging the Bukkit runtime to embed"
        VERBATIM
    )
    add_dependencies(basalt-bukkit-embed basalt-bukkit-runtime)
    add_dependencies(server-bukkit basalt-bukkit-embed)
endfunction()
