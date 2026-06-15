# Copies the pieces of the Bukkit runtime into the folder the server embeds, with an empty file for each one that is missing.
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
    if(EXISTS "${_source}")
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_source}" "${OUT}/${_name}")
    else()
        file(WRITE "${OUT}/${_name}" "")
        message(WARNING "Bukkit runtime: ${_source} is missing, so this binary will not run Bukkit plugins")
    endif()
endforeach()
