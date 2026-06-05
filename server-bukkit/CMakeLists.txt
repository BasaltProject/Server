# Optional: the Bukkit compatibility layer, which embeds Basalt Shim. Off unless BASALT_BUILD_BUKKIT is on.
set(BASALT_JNI_INCLUDE_DIR "" CACHE PATH "Directory that holds jni.h (a JDK include directory)")
if(NOT BASALT_JNI_INCLUDE_DIR)
    file(GLOB BASALT_JDK_INCLUDES /usr/lib/jvm/*/include /Library/Java/JavaVirtualMachines/*/Contents/Home/include)
    find_path(BASALT_JNI_INCLUDE_DIR_FOUND jni.h HINTS $ENV{JAVA_HOME}/include ${BASALT_JDK_INCLUDES})
    set(BASALT_JNI_INCLUDE_DIR "${BASALT_JNI_INCLUDE_DIR_FOUND}")
endif()
if(NOT BASALT_JNI_INCLUDE_DIR)
    message(FATAL_ERROR "BASALT_BUILD_BUKKIT needs jni.h: set BASALT_JNI_INCLUDE_DIR or JAVA_HOME")
endif()

basalt_module(server-bukkit
    SOURCES
        src/BukkitBridge.cpp
        src/BukkitHost.cpp
        src/BukkitNatives.cpp
        src/BukkitWorldNatives.cpp
        src/BukkitPackets.cpp
        src/BukkitWorldNatives.cpp
        src/ShimHost.cpp
    DEPENDS basalt::server-plugins basalt::wire-common
)
target_include_directories(server-bukkit SYSTEM PUBLIC ${BASALT_JNI_INCLUDE_DIR} ${BASALT_JNI_INCLUDE_DIR}/linux
                                                      ${BASALT_JNI_INCLUDE_DIR}/darwin)
target_link_libraries(server-bukkit PUBLIC ${CMAKE_DL_LIBS})

# The Java half: the runtime jar, built against the Bukkit API that server-bukkit/scripts/build-api.sh produced.
set(BASALT_BUKKIT_BUILD_DIR "${CMAKE_SOURCE_DIR}/build/bukkit" CACHE PATH "Where server-bukkit/scripts puts the API and libraries")
add_custom_target(server-bukkit-java ALL
    COMMAND ${CMAKE_SOURCE_DIR}/server-bukkit/scripts/build-java.sh ${BASALT_BUKKIT_BUILD_DIR}
    COMMENT "Building the Bukkit runtime jar"
    VERBATIM
)
