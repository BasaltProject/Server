# The commented defaults an operator gets on first run, compiled in rather than read from a folder
# beside the binary: a release is one self-contained executable.
file(GLOB BASALT_CONFIG_DEFAULTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/resources/config-defaults/*.toml)
basalt_embed_text(server-runtime-config-defaults
    NAMESPACE basalt::server::runtime::config
    HEADER basalt/server/runtime/config/ConfigDefaults.hpp
    SUFFIX DefaultToml
    FILES ${BASALT_CONFIG_DEFAULTS}
)

basalt_module(server-runtime-core
    SOURCES
        src/Connection.cpp
        src/LoginHandler.cpp
        src/config/AnimationsConfig.cpp
        src/config/BasaltConfig.cpp
        src/config/CommandsConfig.cpp
        src/config/CompatibilityConfig.cpp
        src/config/ConfigDirectory.cpp
        src/config/ForwardingConfig.cpp
        src/config/GroupsConfig.cpp
        src/config/LiveConfigs.cpp
        src/config/ProfileSelection.cpp
        src/config/ServerListConfig.cpp
        src/config/UiConfigs.cpp
        src/entity/Aabb.cpp
        src/entity/BlockRay.cpp
        src/entity/EntityCollision.cpp
        src/entity/MobRules.cpp
        src/entity/ServerEntity.cpp
        src/config/WorldsConfig.cpp
        src/function/FunctionLibrary.cpp
        src/net/ProxyLogin.cpp
        src/net/SessionOutbox.cpp
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
        src/world/ChorusFlowerGrowth.cpp
        src/world/RealBlockTags.cpp
        src/world/RealItemTags.cpp
        src/world/RealFluidTags.cpp
        src/world/RealEntityTypeTags.cpp
        src/world/RealTimelineTags.cpp
        src/world/DroppedItemPhysics.cpp
        src/world/EntityRegistry.cpp
        src/world/FluidFlow.cpp
        src/world/ItemEnchantability.cpp
        src/world/NaturalSpawnObstruction.cpp
        src/world/PendingChunkDecorations.cpp
        src/world/PrimedTntQueue.cpp
        src/world/SculkSensorTracker.cpp
        src/world/MobLocalDifficulty.cpp
        src/world/SelectableSlots.cpp
        src/world/GameRules.cpp
        src/world/BambooGrowth.cpp
        src/world/BlockInventory.cpp
        src/world/BlockSurvival.cpp
        src/world/BrewingStands.cpp
        src/world/Buckets.cpp
        src/world/ChiseledBookshelves.cpp
        src/world/ChunkGenerationQueue.cpp
        src/world/ContainerOpeners.cpp
        src/world/Containers.cpp
        src/world/DecoratedPots.cpp
        src/world/DetectorRails.cpp
        src/world/Crafters.cpp
        src/world/DaylightDetectors.cpp
        src/world/DoubleChests.cpp
        src/world/DripstoneGrowth.cpp
        src/world/FallingBlocks.cpp
        src/world/Fires.cpp
        src/world/GrowthSupport.cpp
        src/world/Jukeboxes.cpp
        src/world/LavaGrowth.cpp
        src/world/Lecterns.cpp
        src/world/LocalMobCaps.cpp
        src/world/Sponges.cpp
        src/world/StemGrowth.cpp
        src/world/PlantGrowth.cpp
        src/world/MiscGrowth.cpp
        src/world/MushroomGrowth.cpp
        src/world/NoteBlocks.cpp
        src/world/PoweredRails.cpp
        src/world/PressurePlates.cpp
        src/world/Rails.cpp
        src/world/Reshapes.cpp
        src/world/Redstone.cpp
        src/world/RedstoneComponents.cpp
        src/world/RedstoneReactors.cpp
        src/world/ScheduledBlocks.cpp
        src/world/Shelves.cpp
        src/world/Shulkers.cpp
        src/world/SnowyBlocks.cpp
        src/world/Tripwires.cpp
        src/world/Vibrations.cpp
        src/world/VineGrowth.cpp
        src/world/WorldBorder.cpp
        src/world/WorldLight.cpp
        src/world/WorldClock.cpp
        src/world/RegionStore.cpp
        src/world/WorldMetadata.cpp
        src/world/WorldState.cpp
        src/world/SpawnPotential.cpp
    DEPENDS basalt::server-random basalt::server-registry basalt::server-runtime-config-defaults basalt::server-world basalt::wire-auth basalt::wire-io
            basalt::wire-multiversion basalt::wire-nbt basalt::wire-protocol basalt::wire-text
)

basalt_test(server-runtime-chorus-flower-growth-test
    SOURCES
        test/ChorusFlowerGrowthTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-real-block-tags-test
    SOURCES
        test/RealBlockTagsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-real-item-tags-test
    SOURCES
        test/RealItemTagsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-real-fluid-tags-test
    SOURCES
        test/RealFluidTagsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-real-entity-type-tags-test
    SOURCES
        test/RealEntityTypeTagsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-real-timeline-tags-test
    SOURCES
        test/RealTimelineTagsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-block-ray-test
    SOURCES
        test/BlockRayTest.cpp
    DEPENDS basalt::server-runtime-core
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

basalt_test(server-runtime-config-test
    SOURCES
        test/ConfigDirectoryTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-dropped-items-test
    SOURCES
        test/DroppedItemsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-pending-decorations-test
    SOURCES
        test/PendingDecorationsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-game-rules-test
    SOURCES
        test/GameRulesTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-containers-test
    SOURCES
        test/ContainersTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-container-openers-test
    SOURCES
        test/ContainerOpenersTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-world-border-test
    SOURCES
        test/WorldBorderTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-chiseled-bookshelves-test
    SOURCES
        test/ChiseledBookshelvesTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-brewing-stands-test
    SOURCES
        test/BrewingStandsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-buckets-test
    SOURCES
        test/BucketsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-fires-test
    SOURCES
        test/FiresTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-jukeboxes-test
    SOURCES
        test/JukeboxesTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-lava-growth-test
    SOURCES
        test/LavaGrowthTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-lecterns-test
    SOURCES
        test/LecternsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-local-mob-caps-test
    SOURCES
        test/LocalMobCapsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-pressure-plates-test
    SOURCES
        test/PressurePlatesTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-scheduled-blocks-test
    SOURCES
        test/ScheduledBlocksTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-shelves-test
    SOURCES
        test/ShelvesTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-shulkers-test
    SOURCES
        test/ShulkersTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-snowy-blocks-test
    SOURCES
        test/SnowyBlocksTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-decorated-pots-test
    SOURCES
        test/DecoratedPotsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-detector-rails-test
    SOURCES
        test/DetectorRailsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-world-light-test
    SOURCES
        test/WorldLightTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-block-inventory-test
    SOURCES
        test/BlockInventoryTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-block-survival-test
    SOURCES
        test/BlockSurvivalTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-bamboo-growth-test
    SOURCES
        test/BambooGrowthTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-crafters-test
    SOURCES
        test/CraftersTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-daylight-detectors-test
    SOURCES
        test/DaylightDetectorsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-double-chests-test
    SOURCES
        test/DoubleChestsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-dripstone-growth-test
    SOURCES
        test/DripstoneGrowthTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-tripwires-test
    SOURCES
        test/TripwiresTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-vibrations-test
    SOURCES
        test/VibrationsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-vine-growth-test
    SOURCES
        test/VineGrowthTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-falling-blocks-test
    SOURCES
        test/FallingBlocksTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-fluid-flow-test
    SOURCES
        test/FluidFlowTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-misc-growth-test
    SOURCES
        test/MiscGrowthTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-mushroom-growth-test
    SOURCES
        test/MushroomGrowthTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-note-blocks-test
    SOURCES
        test/NoteBlocksTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-powered-rails-test
    SOURCES
        test/PoweredRailsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-rails-test
    SOURCES
        test/RailsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-redstone-reactors-test
    SOURCES
        test/RedstoneReactorsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-redstone-components-test
    SOURCES
        test/RedstoneComponentsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-reshapes-test
    SOURCES
        test/ReshapesTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-redstone-test
    SOURCES
        test/RedstoneTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-plant-growth-test
    SOURCES
        test/PlantGrowthTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-sponges-test
    SOURCES
        test/SpongesTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-stem-growth-test
    SOURCES
        test/StemGrowthTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-world-clock-test
    SOURCES
        test/WorldClockTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-generation-queue-test
    SOURCES
        test/ChunkGenerationQueueTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-region-store-test
    SOURCES
        test/RegionStoreTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-region-store-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-world-metadata-test
    SOURCES
        test/WorldMetadataTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-world-metadata-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-world-state-test
    SOURCES
        test/WorldStateTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-world-pieces-test
    SOURCES
        test/WorldPiecesTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-world-rules-test
    SOURCES
        test/WorldRulesTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-collision-test
    SOURCES
        test/EntityCollisionTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-mob-rules-test
    SOURCES
        test/MobRulesTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-entity-test
    SOURCES
        test/ServerEntityTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-live-config-test
    SOURCES
        test/LiveConfigsTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-live-config-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-server-list-test
    SOURCES
        test/ServerListConfigTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-server-list-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-ui-config-test
    SOURCES
        test/UiConfigsTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-ui-config-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-profile-test
    SOURCES
        test/ProfileSelectionTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-compat-test
    SOURCES
        test/CompatibilityConfigTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-compat-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-worlds-config-test
    SOURCES
        test/WorldsConfigTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-worlds-config-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-groups-test
    SOURCES
        test/GroupsConfigTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-groups-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-server-config-test
    SOURCES
        test/BasaltConfigTest.cpp
    DEPENDS basalt::server-runtime-core
)

# The tests that write files are told where inside the build tree to put them.
target_compile_definitions(server-runtime-config-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

target_compile_definitions(server-runtime-server-config-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
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

basalt_test(server-runtime-net-test
    SOURCES
        test/SessionNetTest.cpp
    DEPENDS basalt::server-runtime-core
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
