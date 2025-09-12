# Names the final executable the way it is downloaded: BasaltServer-<version>-<platform>-<arch>.

function(basalt_name_binary TARGET)
    if(APPLE)
        set(_platform "macOS")
    elseif(WIN32)
        set(_platform "Windows")
    else()
        set(_platform "Linux")
    endif()

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
        set(_arch "arm64")
    else()
        set(_arch "x64")
    endif()

    set_target_properties(${TARGET} PROPERTIES
        OUTPUT_NAME "BasaltServer-${BASALT_VERSION}-${_platform}-${_arch}"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endfunction()
