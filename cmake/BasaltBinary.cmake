# Names the final executable the way it is downloaded: BasaltServer-<version>-<platform>-<arch>.

function(basalt_name_binary TARGET)
    if(ANDROID)
        set(_platform "Android")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
        set(_platform "FreeBSD")
    elseif(APPLE)
        set(_platform "macOS")
    elseif(WIN32)
        set(_platform "Windows")
    elseif(BASALT_LINUX_MUSL)
        set(_platform "Linux-musl")
    else()
        set(_platform "Linux")
    endif()

    set(_processor "${CMAKE_SYSTEM_PROCESSOR}")
    if(APPLE AND CMAKE_OSX_ARCHITECTURES)
        set(_processor "${CMAKE_OSX_ARCHITECTURES}")
    endif()

    if(_processor MATCHES "^(aarch64|arm64|ARM64)$")
        set(_arch "arm64")
    elseif(_processor MATCHES "^(x86_64|amd64|AMD64|x64)$")
        set(_arch "x64")
    else()
        message(FATAL_ERROR "Unsupported release architecture: ${_processor}")
    endif()

    set_target_properties(${TARGET} PROPERTIES
        OUTPUT_NAME "BasaltServer-${BASALT_VERSION}-${_platform}-${_arch}"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endfunction()
