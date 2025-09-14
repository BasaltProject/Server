basalt_module(server-runtime-core
    SOURCES
        src/Connection.cpp
        src/LoginHandler.cpp
    DEPENDS basalt::wire-auth basalt::wire-io basalt::wire-protocol basalt::wire-text
)

add_executable(basalt-server src/Main.cpp)
target_link_libraries(basalt-server PRIVATE basalt::server-runtime-core basalt::warnings basalt::sanitizers
                                            basalt::parity)
set_target_properties(basalt-server PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
basalt_name_binary(basalt-server)
