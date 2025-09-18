basalt_module(server-registry
    SOURCES
        src/BlockStateDefinitions.cpp
    src/BlockStateFamilies.cpp
        src/BlockConnections.cpp
        src/BlockFluids.cpp
        src/BlockShapes.cpp
        src/BlockSupport.cpp
        src/Blocks.cpp
        src/Entities.cpp
        src/ItemFood.cpp
        src/ItemTools.cpp
        src/Items.cpp
        src/Registries.cpp
        src/Tags.cpp
        src/VanillaBlocks.cpp
        src/VanillaEntries.cpp
        src/World.cpp
        src/WorldTables.cpp
        src/VanillaTables.cpp
    DEPENDS basalt::wire-common basalt::wire-nbt basalt::wire-protocol
)

basalt_test(server-registry-families-test
    SOURCES
        test/FamiliesTest.cpp
    DEPENDS basalt::server-registry
)

basalt_test(server-registry-test
    SOURCES
        test/RegistryTest.cpp
    DEPENDS basalt::server-registry
)
