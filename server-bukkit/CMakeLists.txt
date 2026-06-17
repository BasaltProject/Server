# The Bukkit compatibility layer, which embeds Basalt Shim. Added when cmake/BasaltBukkit.cmake finds a JDK.
set(BASALT_JNI_INCLUDE_DIR "" CACHE PATH "Directory that holds jni.h (a JDK include directory)")
if(NOT BASALT_JNI_INCLUDE_DIR)
    file(GLOB BASALT_JDK_INCLUDES /usr/lib/jvm/*/include /Library/Java/JavaVirtualMachines/*/Contents/Home/include)
    find_path(BASALT_JNI_INCLUDE_DIR_FOUND jni.h HINTS $ENV{JAVA_HOME}/include ${BASALT_JDK_INCLUDES})
    set(BASALT_JNI_INCLUDE_DIR "${BASALT_JNI_INCLUDE_DIR_FOUND}")
endif()
if(NOT BASALT_JNI_INCLUDE_DIR)
    message(FATAL_ERROR "The Bukkit layer needs jni.h: set BASALT_JNI_INCLUDE_DIR or JAVA_HOME")
endif()

basalt_module(server-bukkit
    SOURCES
        src/BukkitBridge.cpp
        src/BukkitEmbedded.cpp
        src/BukkitEntityNatives.cpp
        src/BukkitHost.cpp
        src/BukkitItemNatives.cpp
        src/BukkitMiscNatives.cpp
        src/BukkitNatives.cpp
        src/BukkitPackets.cpp
        src/BukkitPlayerNatives.cpp
        src/BukkitWorldNatives.cpp
        src/ShimHost.cpp
    DEPENDS basalt::server-plugins basalt::wire-common
)
target_include_directories(server-bukkit SYSTEM PUBLIC ${BASALT_JNI_INCLUDE_DIR} ${BASALT_JNI_INCLUDE_DIR}/linux
                                                      ${BASALT_JNI_INCLUDE_DIR}/darwin)
target_link_libraries(server-bukkit PUBLIC ${CMAKE_DL_LIBS})

file(MAKE_DIRECTORY ${BASALT_BUKKIT_EMBED_DIR})
set(BASALT_BUKKIT_EMBEDDED_FILES ${BASALT_BUKKIT_EMBED_DIR}/libjvm ${BASALT_BUKKIT_EMBED_DIR}/boot.jar
    ${BASALT_BUKKIT_EMBED_DIR}/runtime.jar ${BASALT_BUKKIT_EMBED_DIR}/libs.txt)
foreach(_file IN LISTS BASALT_BUKKIT_EMBEDDED_FILES)
    if(NOT EXISTS ${_file})
        file(WRITE ${_file} "")
    endif()
endforeach()
set_source_files_properties(src/BukkitEmbedded.cpp PROPERTIES OBJECT_DEPENDS "${BASALT_BUKKIT_EMBEDDED_FILES}"
                            COMPILE_DEFINITIONS "BASALT_BUKKIT_EMBED_DIR=\"${BASALT_BUKKIT_EMBED_DIR}\"")
