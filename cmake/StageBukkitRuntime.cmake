# Copies the complete Bukkit runtime into the folder the server embeds.
# Usage: cmake -DSHIM=<dir> -DRUNTIME=<dir> -DOUT=<dir> -P StageBukkitRuntime.cmake
if(CMAKE_HOST_APPLE)
    set(_lib libjvm.dylib)
else()
    set(_lib libjvm.so)
endif()
file(MAKE_DIRECTORY "${OUT}")
set(_pieces
    "libjvm|${SHIM}/${_lib}"
    "boot.jar|${SHIM}/classpath.jar"
    "runtime.jar|${RUNTIME}/basalt-bukkit.jar"
    "libs.txt|${RUNTIME}/libs.txt")
foreach(_piece IN LISTS _pieces)
    string(REPLACE "|" ";" _pair "${_piece}")
    list(GET _pair 0 _name)
    list(GET _pair 1 _source)
    if(NOT EXISTS "${_source}" OR IS_DIRECTORY "${_source}")
        message(FATAL_ERROR "Bukkit runtime: required file ${_source} is missing; see the Shim and Bukkit build logs")
    endif()
    file(SIZE "${_source}" _size)
    if(_size EQUAL 0)
        message(FATAL_ERROR "Bukkit runtime: required file ${_source} is empty; see the Shim and Bukkit build logs")
    endif()
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_source}" "${OUT}/${_name}"
                    COMMAND_ERROR_IS_FATAL ANY)
endforeach()
