basalt_module(server-storage
    SOURCES
        src/AccessRepository.cpp
        src/PlayerRepository.cpp
        src/PlayerStore.cpp
        src/PluginDataRepository.cpp
        src/ProgressRepository.cpp
        src/Schema.cpp
        src/StorageNbt.cpp
        src/StoredPlayer.cpp
        src/VanillaPlayerData.cpp
    DEPENDS basalt::wire-common basalt::wire-nbt basalt::wire-sqlite
)

basalt_test(server-storage-test
    SOURCES
        test/StorageTest.cpp
    DEPENDS basalt::server-storage
)

basalt_test(server-storage-database-test
    SOURCES
        test/PlayerStoreTest.cpp
    DEPENDS basalt::server-storage
)

# The only test here that writes a file, so it is told where inside the build tree to put it: no
# test writes outside build/, and running the binary by hand from the repo root must not either.
target_compile_definitions(server-storage-database-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)
