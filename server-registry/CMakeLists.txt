basalt_module(server-registry
    SOURCES
        src/BlockStateDefinitions.cpp
        src/Blocks.cpp
        src/Registries.cpp
        src/VanillaBlocks.cpp
        src/VanillaTables.cpp
    DEPENDS basalt::wire-common basalt::wire-nbt basalt::wire-protocol
)

basalt_test(server-registry-test
    SOURCES
        test/RegistryTest.cpp
    DEPENDS basalt::server-registry
)
