basalt_module(server-runtime-core
    SOURCES
        src/Connection.cpp
        src/LoginHandler.cpp
        src/registry/VanillaDimensionTypes.cpp
        src/registry/VanillaRegistryData.cpp
        src/registry/VanillaTagData.cpp
    DEPENDS basalt::wire-auth basalt::wire-io basalt::wire-nbt basalt::wire-protocol basalt::wire-text
)

basalt_test(server-runtime-registry-test
    SOURCES
        test/RegistryDataTest.cpp
    DEPENDS basalt::server-runtime-core
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
