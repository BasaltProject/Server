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
        src/entity/EntityPhysics.cpp
        src/entity/FlyingMovement.cpp
        src/entity/LivingMovement.cpp
        src/entity/PathFinder.cpp
        src/entity/Projectiles.cpp
        src/entity/EntityNbt.cpp
        src/entity/EntityCollision.cpp
        src/entity/MobRules.cpp
        src/entity/ServerEntity.cpp
        src/config/WorldsConfig.cpp
        src/command/StringReader.cpp
        src/command/ArgumentTypes.cpp
        src/command/commands/BasicCommands.cpp
        src/TickLoop.cpp
        src/command/Coordinates.cpp
        src/command/RegistryArguments.cpp
        src/command/commands/GroupCommands.cpp
        src/command/commands/HudCommands.cpp
        src/command/commands/RandomCommand.cpp
        src/command/commands/PlayerCommands.cpp
        src/command/commands/SaveCommands.cpp
        src/command/commands/StopwatchCommand.cpp
        src/command/commands/TickCommand.cpp
        src/command/commands/ScoreCommands.cpp
        src/command/commands/SessionCommands.cpp
        src/command/commands/WorldCommands.cpp
        src/command/CommandTree.cpp
        src/command/EntitySelector.cpp
        src/command/SelectorParser.cpp
        src/command/SelectorOptions.cpp
        src/command/CommandDispatcher.cpp
        src/command/CommandEnvironment.cpp
        src/command/CommandTarget.cpp
        src/command/ExecutionSource.cpp
        src/command/CommandSource.cpp
        src/command/MessageOverrides.cpp
        src/placeholder/PlaceholderSlots.cpp
        src/item/ItemWear.cpp
        src/item/ItemComponentCodecs.cpp
        src/item/ItemDamageEffects.cpp
        src/item/ItemComponentPayloads.cpp
        src/function/FunctionLibrary.cpp
        src/player/FoodData.cpp
        src/player/PlayerSession.cpp
        src/player/PlayerPersistence.cpp
        src/player/PlayerRegistry.cpp
        src/player/Menu.cpp
        src/player/PlayerInventory.cpp
        src/player/Combat.cpp
        src/player/ExperienceData.cpp
        src/player/PlayerStats.cpp
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
        src/world/RealBiomeSpawns.cpp
        src/world/RealBlockTags.cpp
        src/world/RealItemTags.cpp
        src/world/RealFluidTags.cpp
        src/world/RealEntityTypeTags.cpp
        src/world/RealTimelineTags.cpp
        src/world/SaplingGrowth.cpp
        src/world/RealLootTables.cpp
        src/world/RealEnchantments.cpp
        src/world/RealDamageTypeTags.cpp
        src/world/RealDialogs.cpp
        src/world/RealTestInstances.cpp
        src/world/RealNoiseParameters.cpp
        src/world/BlockInteractions.cpp
        src/world/ProjectileHits.cpp
        src/world/Furnaces.cpp
        src/world/Hoppers.cpp
        src/world/EndPortals.cpp
        src/world/EnchantmentAttributes.cpp
        src/world/EndPodium.cpp
        src/world/NetherPortalGrowth.cpp
        src/world/DamageProtection.cpp
        src/world/NaturalSpawnPredicates.cpp
        src/world/BiomeTagLoader.cpp
        src/world/Dispensers.cpp
        src/world/PortalIndex.cpp
        src/world/RealTimelines.cpp
        src/world/StructureTemplates.cpp
        src/world/BlockPlacement.cpp
        src/world/RealRecipes.cpp
        src/world/Pistons.cpp
        src/world/NeighbourUpdates.cpp
        src/world/RealVillagerTrades.cpp
        src/world/RuinedPortalVariants.cpp
        src/world/RealSurfaceEnvironment.cpp
        src/world/NetherSurfaceEnvironment.cpp
        src/world/Campfires.cpp
        src/world/BlockEntityTicks.cpp
        src/world/Walls.cpp
        src/world/RedstoneWire.cpp
        src/world/Geysers.cpp
        src/world/Signs.cpp
        src/world/BiomeZoomSeed.cpp
        src/world/Beds.cpp
        src/world/ClimbingPlantGrowth.cpp
        src/world/Fluids.cpp
        src/world/MobEffects.cpp
        src/world/Lightning.cpp
        src/world/Explosions.cpp
        src/world/AdvancementTriggers.cpp
        src/world/Anvils.cpp
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
        src/world/LoadedWorld.cpp
        src/world/LocalMobCaps.cpp
        src/world/Sponges.cpp
        src/world/StemGrowth.cpp
        src/world/MiscGrowth.cpp
        src/world/MushroomGrowth.cpp
        src/world/NoteBlocks.cpp
        src/world/PlayerProgress.cpp
        src/world/PoweredRails.cpp
        src/world/PressurePlates.cpp
        src/world/Rails.cpp
        src/world/RandomTicks.cpp
        src/world/Reshapes.cpp
        src/world/RecipeBook.cpp
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
        src/world/WorldSpawnPoint.cpp
        src/world/WorldState.cpp
        src/world/SpawnPotential.cpp
    DEPENDS basalt::server-random basalt::server-registry basalt::server-runtime-config-defaults basalt::server-storage basalt::server-world basalt::wire-auth basalt::wire-io
            basalt::wire-multiversion basalt::wire-nbt basalt::wire-protocol basalt::wire-text
)

target_compile_definitions(server-runtime-core
    PUBLIC BASALT_VERSION="${BASALT_VERSION}"
)

basalt_test(server-runtime-chorus-flower-growth-test
    SOURCES
        test/ChorusFlowerGrowthTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-real-biome-spawns-test
    SOURCES
        test/RealBiomeSpawnsTest.cpp
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

basalt_test(server-runtime-sapling-growth-test
    SOURCES
        test/SaplingGrowthTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-path-finder-test
    SOURCES
        test/PathFinderTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-living-movement-test
    SOURCES
        test/LivingMovementTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-flying-movement-test
    SOURCES
        test/FlyingMovementTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-entity-selector-test
    SOURCES
        test/EntitySelectorTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-entity-nbt-test
    SOURCES
        test/EntityNbtTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-entity-physics-test
    SOURCES
        test/EntityPhysicsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-projectiles-test
    SOURCES
        test/ProjectilesTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-real-loot-tables-test
    SOURCES
        test/RealLootTablesTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-real-enchantments-test
    SOURCES
        test/RealEnchantmentsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-real-damage-type-tags-test
    SOURCES
        test/RealDamageTypeTagsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-real-dialogs-test
    SOURCES
        test/RealDialogsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-real-test-instances-test
    SOURCES
        test/RealTestInstancesTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-real-noise-parameters-test
    SOURCES
        test/RealNoiseParametersTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-biome-zoom-seed-test
    SOURCES
        test/BiomeZoomSeedTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-block-interactions-test
    SOURCES
        test/BlockInteractionsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-projectile-hits-test
    SOURCES
        test/ProjectileHitsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-combat-test
    SOURCES
        test/CombatTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-experience-data-test
    SOURCES
        test/ExperienceDataTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-coordinates-test
    SOURCES
        test/CoordinatesTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-registry-arguments-test
    SOURCES
        test/RegistryArgumentsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-group-commands-test
    SOURCES
        test/GroupCommandsTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-group-commands-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-hud-commands-test
    SOURCES
        test/HudCommandsTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-hud-commands-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-score-commands-test
    SOURCES
        test/ScoreCommandsTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-score-commands-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-session-commands-test
    SOURCES
        test/SessionCommandsTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-session-commands-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-tick-command-test
    SOURCES
        test/TickCommandTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-tick-command-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-world-commands-test
    SOURCES
        test/WorldCommandsTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-world-commands-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-player-commands-test
    SOURCES
        test/PlayerCommandsTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-player-commands-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-player-inventory-test
    SOURCES
        test/PlayerInventoryTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-player-progress-test
    SOURCES
        test/PlayerProgressTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-player-session-test
    SOURCES
        test/PlayerSessionTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-player-stats-test
    SOURCES
        test/PlayerStatsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-food-data-test
    SOURCES
        test/FoodDataTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-furnaces-test
    SOURCES
        test/FurnacesTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-hoppers-test
    SOURCES
        test/HoppersTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-enchantment-attributes-test
    SOURCES
        test/EnchantmentAttributesTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-end-portals-test
    SOURCES
        test/EndPortalsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-end-podium-test
    SOURCES
        test/EndPodiumTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-nether-portal-growth-test
    SOURCES
        test/NetherPortalGrowthTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-damage-protection-test
    SOURCES
        test/DamageProtectionTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-natural-spawn-predicates-test
    SOURCES
        test/NaturalSpawnPredicatesTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-biome-tag-loader-test
    SOURCES
        test/BiomeTagLoaderTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-dispensers-test
    SOURCES
        test/DispensersTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-portal-index-test
    SOURCES
        test/PortalIndexTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-portal-index-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-real-timelines-test
    SOURCES
        test/RealTimelinesTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-structure-templates-test
    SOURCES
        test/StructureTemplatesTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-block-placement-test
    SOURCES
        test/BlockPlacementTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-real-recipes-test
    SOURCES
        test/RealRecipesTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-pistons-test
    SOURCES
        test/PistonsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-neighbour-updates-test
    SOURCES
        test/NeighbourUpdatesTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-real-villager-trades-test
    SOURCES
        test/RealVillagerTradesTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-ruined-portal-variants-test
    SOURCES
        test/RuinedPortalVariantsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-real-surface-environment-test
    SOURCES
        test/RealSurfaceEnvironmentTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-nether-surface-environment-test
    SOURCES
        test/NetherSurfaceEnvironmentTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-campfires-test
    SOURCES
        test/CampfiresTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-block-entity-ticks-test
    SOURCES
        test/BlockEntityTicksTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-item-component-codecs-test
    SOURCES
        test/ItemComponentCodecsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-item-damage-effects-test
    SOURCES
        test/ItemDamageEffectsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-item-wear-test
    SOURCES
        test/ItemWearTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-string-reader-test
    SOURCES
        test/StringReaderTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-walls-test
    SOURCES
        test/WallsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-recipe-book-test
    SOURCES
        test/RecipeBookTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-redstone-wire-test
    SOURCES
        test/RedstoneWireTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-geysers-test
    SOURCES
        test/GeysersTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-signs-test
    SOURCES
        test/SignsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-basic-commands-test
    SOURCES
        test/BasicCommandsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-beds-test
    SOURCES
        test/BedsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-climbing-plant-growth-test
    SOURCES
        test/ClimbingPlantGrowthTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-fluids-test
    SOURCES
        test/FluidsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-menu-test
    SOURCES
        test/MenuTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-message-overrides-test
    SOURCES
        test/MessageOverridesTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-mob-effects-test
    SOURCES
        test/MobEffectsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-lightning-test
    SOURCES
        test/LightningTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-explosions-test
    SOURCES
        test/ExplosionsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-advancement-triggers-test
    SOURCES
        test/AdvancementTriggersTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-argument-types-test
    SOURCES
        test/ArgumentTypesTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-argument-types-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-anvils-test
    SOURCES
        test/AnvilsTest.cpp
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

basalt_test(server-runtime-command-dispatcher-test
    SOURCES
        test/CommandDispatcherTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-command-environment-test
    SOURCES
        test/CommandEnvironmentTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-command-environment-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-command-source-test
    SOURCES
        test/CommandSourceTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-command-target-test
    SOURCES
        test/CommandTargetTest.cpp
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

basalt_test(server-runtime-loaded-world-test
    SOURCES
        test/LoadedWorldTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-loaded-world-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
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

basalt_test(server-runtime-random-ticks-test
    SOURCES
        test/RandomTicksTest.cpp
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

basalt_test(server-runtime-world-spawn-point-test
    SOURCES
        test/WorldSpawnPointTest.cpp
    DEPENDS basalt::server-runtime-core
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
