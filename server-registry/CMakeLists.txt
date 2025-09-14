basalt_module(server-registry
    SOURCES
        src/BlockStateDefinitions.cpp
        src/Blocks.cpp
        src/VanillaBlocks.cpp
    DEPENDS basalt::wire-common basalt::wire-nbt
)

basalt_test(server-registry-test
    SOURCES
        test/RegistryTest.cpp
    DEPENDS basalt::server-registry
)
