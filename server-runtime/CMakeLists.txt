basalt_module(server-runtime-core
    SOURCES
        src/Connection.cpp
        src/LoginHandler.cpp
        src/function/FunctionLibrary.cpp
        src/registry/VanillaBiomes.cpp
        src/registry/VanillaConfigurationRegistries.cpp
        src/registry/VanillaDimensionTypes.cpp
        src/registry/VanillaNetherEndBiomes.cpp
        src/registry/VanillaOverworldBiomesA.cpp
        src/registry/VanillaOverworldBiomesB.cpp
        src/registry/VanillaRegistryData.cpp
        src/registry/VanillaTagData.cpp
        src/sched/Owner.cpp
        src/sched/RegionMap.cpp
        src/sched/RegionScheduler.cpp
        src/sched/RegionWorkers.cpp
        src/scoreboard/Scoreboard.cpp
        src/scoreboard/Teams.cpp
        src/timings/TimingsReport.cpp
    DEPENDS basalt::wire-auth basalt::wire-io basalt::wire-nbt basalt::wire-protocol basalt::wire-text
)

basalt_test(server-runtime-biome-registry-test
    SOURCES
        test/BiomeRegistryTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-timings-test
    SOURCES
        test/TimingsReportTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-function-test
    SOURCES
        test/FunctionLibraryTest.cpp
    DEPENDS basalt::server-runtime-core
)

# The only test here that writes files, so it is told where inside the build tree to put them.
target_compile_definitions(server-runtime-function-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-region-test
    SOURCES
        test/RegionSchedulerTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-sched-test
    SOURCES
        test/SchedulerTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-scoreboard-test
    SOURCES
        test/ScoreboardTest.cpp
    DEPENDS basalt::server-runtime-core
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
