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
        src/bedrock/BedrockBlockEntities.cpp
        src/bedrock/BedrockChunks.cpp
        src/bedrock/BedrockListener.cpp
        src/bedrock/BedrockPlayer.cpp
        src/bedrock/BedrockSeen.cpp
        src/bedrock/BedrockPlayerConnection.cpp
        src/bedrock/BedrockRecipes.cpp
        src/bedrock/BedrockStackRequests.cpp
        src/bedrock/BedrockWindows.cpp
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
        src/entity/BlockTriggers.cpp
        src/entity/EntityPhysics.cpp
        src/entity/FlyingMovement.cpp
        src/entity/LivingMovement.cpp
        src/entity/PathFinder.cpp
        src/entity/Minecarts.cpp
        src/entity/PrimedTnt.cpp
        src/entity/Projectiles.cpp
        src/entity/EntityNbt.cpp
        src/entity/EntityCollision.cpp
        src/entity/MobRules.cpp
        src/entity/GeneratedEntityPresentation.cpp
        src/entity/Villagers.cpp
        src/entity/Raids.cpp
        src/entity/WanderingTraderLifetime.cpp
        src/entity/ServerEntity.cpp
        src/config/WorldsConfig.cpp
        src/command/StringReader.cpp
        src/command/ArgumentTypes.cpp
        src/command/commands/AccessCommands.cpp
        src/command/commands/AttributeCommand.cpp
        src/command/commands/BasicCommands.cpp
        src/TickLoop.cpp
        src/command/BlockEditor.cpp
        src/command/CommandStorage.cpp
        src/command/Coordinates.cpp
        src/command/DataPaths.cpp
        src/command/ExecuteArguments.cpp
        src/command/ParticleArgument.cpp
        src/command/RegistryArguments.cpp
        src/command/VanillaCommands.cpp
        src/command/WorldArguments.cpp
        src/command/commands/BlockCommands.cpp
        src/command/commands/BossBarCommand.cpp
        src/command/commands/DataCommand.cpp
        src/command/commands/DataModify.cpp
        src/command/commands/EffectCommand.cpp
        src/command/commands/EntityCommands.cpp
        src/command/commands/ExecuteCommand.cpp
        src/command/commands/ExecuteConditions.cpp
        src/command/commands/ExecuteContext.cpp
        src/command/commands/FunctionCommands.cpp
        src/command/commands/GroupCommands.cpp
        src/command/commands/HudCommands.cpp
        src/command/commands/PlaceCommand.cpp
        src/command/commands/ProgressCommands.cpp
        src/command/commands/RandomCommand.cpp
        src/command/commands/ImportPlayersCommand.cpp
        src/command/commands/ItemCommand.cpp
        src/command/commands/ItemCommands.cpp
        src/command/commands/ItemSlots.cpp
        src/command/commands/PlaceholderCommand.cpp
        src/command/commands/LocateCommand.cpp
        src/command/commands/LootCommand.cpp
        src/command/commands/ParticleCommand.cpp
        src/command/commands/PlayerCommands.cpp
        src/command/commands/ProfilingCommands.cpp
        src/command/commands/RegionCommands.cpp
        src/command/commands/ReloadCommand.cpp
        src/command/commands/SaveCommands.cpp
        src/command/commands/StopwatchCommand.cpp
        src/command/commands/TagCommand.cpp
        src/command/commands/TestCommand.cpp
        src/command/commands/TeamCommand.cpp
        src/command/commands/TeleportCommand.cpp
        src/command/commands/TeleportMove.cpp
        src/command/commands/TickCommand.cpp
        src/command/commands/TimeCommand.cpp
        src/command/commands/TimingsCommand.cpp
        src/command/commands/ScoreCommands.cpp
        src/command/commands/ScoreboardCommand.cpp
        src/command/commands/ScoreboardObjectives.cpp
        src/command/commands/ScoreboardPlayers.cpp
        src/command/commands/SessionCommands.cpp
        src/command/commands/WaypointCommand.cpp
        src/command/commands/WorldBorderCommand.cpp
        src/command/commands/WorldCommands.cpp
        src/command/CommandTree.cpp
        src/command/EntitySelector.cpp
        src/command/SelectorParser.cpp
        src/command/SelectorOptions.cpp
        src/BlockChangeBroadcaster.cpp
        src/BasaltServer.cpp
        src/BasaltServerConnections.cpp
        src/BasaltServerLife.cpp
        src/BasaltServerTick.cpp
        src/BasaltServerScores.cpp
        src/BlockInteractionHandler.cpp
        src/BlockUseHelpers.cpp
        src/BlockUseOn.cpp
        src/plugins/PacketNames.cpp
        src/plugins/PluginsCommand.cpp
        src/plugins/ServerNpcs.cpp
        src/plugins/ServerPluginServices.cpp
        src/plugins/ServerObjectModel.cpp
        src/plugins/ServerWorldModel.cpp
        src/ChunkSimulationView.cpp
        src/ClientSession.cpp
        src/CommandWiring.cpp
        src/SessionLogin.cpp
        src/SessionPlay.cpp
        src/SessionDelegates.cpp
        src/ChunkStreamer.cpp
        src/CombatAttack.cpp
        src/CombatHandler.cpp
        src/CombatSystem.cpp
        src/ContainerClicks.cpp
        src/ContainerMenuController.cpp
        src/ContainerAnvil.cpp
        src/ContainerBeacon.cpp
        src/ContainerLoom.cpp
        src/ContainerGrindstone.cpp
        src/ContainerEnchantment.cpp
        src/ContainerSlots.cpp
        src/MobActionSystem.cpp
        src/ItemUseTimers.cpp
        src/PlayerLifecycle.cpp
        src/VillagerTradeController.cpp
        src/UiStateSync.cpp
        src/RedstoneTickSystem.cpp
        src/BlockEventTick.cpp
        src/ScheduledBlockTick.cpp
        src/MobTickSystem.cpp
        src/MobActionTick.cpp
        src/MobActionConversion.cpp
        src/MobActionWarden.cpp
        src/MobDeathLoot.cpp
        src/MobActionCopperGolem.cpp
        src/ShriekerSystem.cpp
        src/LightningBoltSystem.cpp
        src/NaturalSpawningMaterialize.cpp
        src/NaturalSpawningSystem.cpp
        src/RaidSystem.cpp
        src/EntityVisibilityExtras.cpp
        src/EntityVisibilityTracker.cpp
        src/EnvironmentTickSystem.cpp
        src/ExplosionSystem.cpp
        src/EndFightSystem.cpp
        src/ExperienceOrbSystem.cpp
        src/FishingSystem.cpp
        src/BossBarView.cpp
        src/WitherSystem.cpp
        src/EndFightArena.cpp
        src/EndFightRespawn.cpp
        src/AreaEffectCloudSystem.cpp
        src/Favicon.cpp
        src/PersistenceCoordinator.cpp
        src/ServerListStatus.cpp
        src/ServerMetrics.cpp
        src/StatusEffectSystem.cpp
        src/VanillaGameData.cpp
        src/WorldRegistry.cpp
        src/SubstituteLabels.cpp
        src/command/CommandDispatcher.cpp
        src/command/CommandEnvironment.cpp
        src/command/ConsoleReader.cpp
        src/command/CommandTarget.cpp
        src/command/ExecutionSource.cpp
        src/command/CommandSource.cpp
        src/command/MessageOverrides.cpp
        src/placeholder/AnimationPlaceholders.cpp
        src/placeholder/Decimals.cpp
        src/placeholder/PlaceholderSlots.cpp
        src/placeholder/PlayerPlaceholders.cpp
        src/placeholder/ServerPlaceholders.cpp
        src/item/ItemWear.cpp
        src/item/ItemComponentCodecs.cpp
        src/item/ItemDamageEffects.cpp
        src/item/ItemComponentPayloads.cpp
        src/function/FunctionLibrary.cpp
        src/player/FoodData.cpp
        src/player/PlayerMovementStats.cpp
        src/player/PlayerSession.cpp
        src/player/PlayerPersistence.cpp
        src/player/PlayerBlockUpdates.cpp
        src/player/PlayerDeath.cpp
        src/player/PlayerBlockBreak.cpp
        src/player/PlayerBlockItemUse.cpp
        src/player/PlayerBlockSlots.cpp
        src/player/PlayerAnvil.cpp
        src/player/PlayerSigns.cpp
        src/player/PlayerBooks.cpp
        src/player/PlayerCooldowns.cpp
        src/player/PlayerPick.cpp
        src/player/PlayerBlockToggle.cpp
        src/player/PlayerBuckets.cpp
        src/player/PlayerDigSpeed.cpp
        src/player/PlayerEating.cpp
        src/player/PlayerItemPickup.cpp
        src/player/PlayerSleep.cpp
        src/player/PlayerSpawnItems.cpp
        src/player/PlayerSweep.cpp
        src/player/PlayerThrown.cpp
        src/player/PlayerVitals.cpp
        src/player/PlayerBlockPlace.cpp
        src/player/PlayerContainers.cpp
        src/player/PlayerDamage.cpp
        src/player/PlayerParticles.cpp
        src/player/PlayerSounds.cpp
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
        src/scoreboard/TeamNameTags.cpp
        src/scoreboard/Teams.cpp
        src/ui/PlayerUi.cpp
        src/timings/TimingsReport.cpp
        src/world/ChorusFlowerGrowth.cpp
        src/world/SummonAwards.cpp
        src/world/RealBannerPatterns.cpp
        src/world/RealBiomeSpawns.cpp
        src/world/RealBlockTags.cpp
        src/world/RealItemTags.cpp
        src/world/RealFluidTags.cpp
        src/world/RealEntityTypeTags.cpp
        src/world/RealTimelineTags.cpp
        src/world/SaplingGrowth.cpp
        src/world/RealLootTables.cpp
        src/world/LootRegistries.cpp
        src/world/ContainerLoot.cpp
        src/world/RealEnchantments.cpp
        src/world/RealEnchantmentTags.cpp
        src/world/RealDamageTypeTags.cpp
        src/world/Dialogs.cpp
        src/world/RealDialogs.cpp
        src/world/RealTestInstances.cpp
        src/world/RealNoiseParameters.cpp
        src/world/BlockInteractions.cpp
        src/world/ProjectileHits.cpp
        src/world/Furnaces.cpp
        src/world/ArrowButtons.cpp
        src/world/Hoppers.cpp
        src/world/EndPortals.cpp
        src/world/EnchantmentAttributes.cpp
        src/world/BlockExperience.cpp
        src/world/BlockPatterns.cpp
        src/world/WitherPatterns.cpp
        src/world/SculkShriekers.cpp
        src/world/WardenSpawnTracker.cpp
        src/world/EndGateways.cpp
        src/world/EndPodium.cpp
        src/world/EndCrystals.cpp
        src/world/DragonFight.cpp
        src/entity/EnderDragon.cpp
        src/entity/DragonNavigation.cpp
        src/entity/DragonPhases.cpp
        src/entity/DragonFlight.cpp
        src/entity/AreaEffectClouds.cpp
        src/world/NetherPortalGrowth.cpp
        src/world/DamageProtection.cpp
        src/world/NaturalSpawnPredicates.cpp
        src/world/BiomeTagLoader.cpp
        src/world/CommandBlocks.cpp
        src/world/Dispensers.cpp
        src/world/Dripleaves.cpp
        src/world/DroppedItemNbt.cpp
        src/world/PortalIndex.cpp
        src/world/RealTimelines.cpp
        src/world/StructureTemplates.cpp
        src/world/BlockPlacement.cpp
        src/world/RealRecipes.cpp
        src/world/PlacementVariants.cpp
        src/world/PistonSlide.cpp
        src/world/Pistons.cpp
        src/world/NeighbourUpdates.cpp
        src/world/RealVillagerTrades.cpp
        src/world/RuinedPortalVariants.cpp
        src/world/RealSurfaceEnvironment.cpp
        src/world/NetherSurfaceEnvironment.cpp
        src/world/Campfires.cpp
        src/world/BlockEntityTicks.cpp
        src/world/Walls.cpp
        src/world/Waxing.cpp
        src/world/WindBursts.cpp
        src/world/RedstoneWire.cpp
        src/world/Geysers.cpp
        src/world/GolemPatterns.cpp
        src/world/CopperChests.cpp
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
        src/world/Beacons.cpp
        src/world/Books.cpp
        src/world/AnvilEnchantments.cpp
        src/world/EnchantmentBooks.cpp
        src/world/Enchanting.cpp
        src/world/Grindstones.cpp
        src/world/Looms.cpp
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
        src/world/BlockEntityDefaults.cpp
        src/world/BoneMeal.cpp
        src/world/BlockInventory.cpp
        src/world/BlockSurvival.cpp
        src/world/BrewingStands.cpp
        src/world/Buckets.cpp
        src/world/ChiseledBookshelves.cpp
        src/world/ChunkGenerationQueue.cpp
        src/world/ContainerOpeners.cpp
        src/world/Containers.cpp
        src/world/ComparatorReading.cpp
        src/world/Composters.cpp
        src/world/DecoratedPots.cpp
        src/world/DetectorRails.cpp
        src/world/Crafters.cpp
        src/world/DaylightDetectors.cpp
        src/world/DoubleChests.cpp
        src/world/DripstoneGrowth.cpp
        src/world/FallingBlocks.cpp
        src/world/Fires.cpp
        src/world/GrowthSupport.cpp
        src/world/ItemFrames.cpp
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
        src/world/RespawnAnchors.cpp
        src/world/RandomSequences.cpp
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
        src/world/StuckSpeed.cpp
        src/world/Tripwires.cpp
        src/world/Vibrations.cpp
        src/world/VineGrowth.cpp
        src/world/WorldBorder.cpp
        src/world/WorldLight.cpp
        src/world/WorldClock.cpp
        src/world/RegionStore.cpp
        src/world/WorldMetadata.cpp
        src/world/WorldSpawnPoint.cpp
        src/world/CustomSpawners.cpp
        src/world/MobSpawnAttributes.cpp
        src/world/MobSpawnFinalization.cpp
        src/world/MobSpawnFinalizers.cpp
        src/world/MobSpawnForest.cpp
        src/world/MobVariantSpawnFinalizer.cpp
        src/world/NaturalSpawner.cpp
        src/world/NetherPortals.cpp
        src/world/NoiseParametersJson.cpp
        src/world/OriginalMobSpawner.cpp
        src/world/MobSpawnVariantData.cpp
        src/world/SpawnEquipmentEnchantments.cpp
        src/world/SpawnEquipmentFinalizer.cpp
        src/world/SkeletonHorseTrap.cpp
        src/world/StructureSpawnLists.cpp
        src/world/ZombieSpawnFinalizer.cpp
        src/world/Trades.cpp
        src/world/VanillaSaveImport.cpp
        src/world/VanillaWorldGeneratorFactory.cpp
        src/entity/Breeding.cpp
        src/entity/DeathDrops.cpp
        src/entity/FishingHooks.cpp
        src/entity/WitherBoss.cpp
        src/entity/SwimPath.cpp
        src/entity/MobBreathing.cpp
        src/entity/MobConversions.cpp
        src/entity/MobStatusEffects.cpp
        src/entity/WardenBrain.cpp
        src/entity/CopperGolem.cpp
        src/entity/EntityCombat.cpp
        src/entity/MobBrain.cpp
        src/entity/MobCombat.cpp
        src/entity/ai/Controls.cpp
        src/entity/ai/FlyingGoals.cpp
        src/entity/ai/GoalSelector.cpp
        src/entity/ai/Goals.cpp
        src/entity/ai/IllagerGoals.cpp
        src/entity/ai/LongDistancePatrolGoal.cpp
        src/entity/ai/MobContext.cpp
        src/entity/ai/MobGoals.cpp
        src/entity/ai/SpecialGoals.cpp
        src/entity/ai/TameGoals.cpp
        src/entity/ai/TargetGoals.cpp
        src/entity/ai/VillagerGoals.cpp
        src/entity/ai/VillagerSchedule.cpp
        src/entity/ai/WanderingTraderGoals.cpp
        src/entity/ai/WitherGoals.cpp
        src/entity/ai/DolphinGoals.cpp
        src/entity/ai/CopperGolemGoals.cpp
        src/world/VillageDataLoader.cpp
        src/world/VillageGolemSpawner.cpp
        src/world/VillageProximity.cpp
        src/world/WeaponDamage.cpp
        src/world/WorldState.cpp
        src/world/SpawnPotential.cpp
    DEPENDS basalt::server-assets basalt::server-lua basalt::wire-bedrock basalt::server-plugins basalt::server-random basalt::server-registry basalt::server-runtime-config-defaults basalt::server-storage basalt::server-world basalt::wire-auth basalt::wire-io
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

basalt_test(server-runtime-dialogs-test
    SOURCES
        test/DialogsTest.cpp
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

basalt_test(server-runtime-vanilla-commands-test
    SOURCES
        test/VanillaCommandsTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-vanilla-commands-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-world-arguments-test
    SOURCES
        test/WorldArgumentsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-progress-commands-test
    SOURCES
        test/ProgressCommandsTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-progress-commands-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-region-commands-test
    SOURCES
        test/RegionCommandsTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-region-commands-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-registry-arguments-test
    SOURCES
        test/RegistryArgumentsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-access-commands-test
    SOURCES
        test/AccessCommandsTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-access-commands-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-block-commands-test
    SOURCES
        test/BlockCommandsTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-block-commands-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-boss-bar-command-test
    SOURCES
        test/BossBarCommandTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-boss-bar-command-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-data-command-test
    SOURCES
        test/DataCommandTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-data-command-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-data-paths-test
    SOURCES
        test/DataPathsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-effect-command-test
    SOURCES
        test/EffectCommandTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-effect-command-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-entity-commands-test
    SOURCES
        test/EntityCommandsTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-entity-commands-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-execute-command-test
    SOURCES
        test/ExecuteCommandTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-execute-command-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-function-commands-test
    SOURCES
        test/FunctionCommandsTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-function-commands-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-group-commands-test
    SOURCES
        test/GroupCommandsTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-group-commands-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-flying-goals-test
    SOURCES
        test/FlyingGoalsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-long-distance-patrol-test
    SOURCES
        test/LongDistancePatrolGoalTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-wandering-trader-goals-test
    SOURCES
        test/WanderingTraderGoalsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-illager-goals-test
    SOURCES
        test/IllagerGoalsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-mob-goals-test
    SOURCES
        test/MobGoalsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-special-goals-test
    SOURCES
        test/SpecialGoalsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-tame-goals-test
    SOURCES
        test/TameGoalsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-villager-goals-test
    SOURCES
        test/VillagerGoalsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-target-goals-test
    SOURCES
        test/TargetGoalsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-goals-test
    SOURCES
        test/GoalsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-breeding-test
    SOURCES
        test/BreedingTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-controls-test
    SOURCES
        test/ControlsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-goal-selector-test
    SOURCES
        test/GoalSelectorTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-world-registry-test
    SOURCES
        test/WorldRegistryTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-world-registry-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/scratch")

basalt_test(server-runtime-vanilla-world-generator-factory-test
    SOURCES
        test/VanillaWorldGeneratorFactoryTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-nether-portals-test
    SOURCES
        test/NetherPortalsTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-nether-portals-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/scratch")

basalt_test(server-runtime-village-golem-spawner-test
    SOURCES
        test/VillageGolemSpawnerTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-village-golem-spawner-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/scratch")

basalt_test(server-runtime-custom-spawners-test
    SOURCES
        test/CustomSpawnersTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-custom-spawners-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/scratch")

basalt_test(server-runtime-natural-spawner-test
    SOURCES
        test/NaturalSpawnerTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-natural-spawner-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/scratch")

basalt_test(server-runtime-mob-spawn-finalization-test
    SOURCES
        test/MobSpawnFinalizationTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-spawn-equipment-test
    SOURCES
        test/SpawnEquipmentTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-mob-spawn-forest-test
    SOURCES
        test/MobSpawnForestTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-trades-test
    SOURCES
        test/TradesTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-mob-spawn-attributes-test
    SOURCES
        test/MobSpawnAttributesTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-server-list-status-test
    SOURCES
        test/ServerListStatusTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-server-list-status-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/scratch")

basalt_test(server-runtime-generated-entity-presentation-test
    SOURCES
        test/GeneratedEntityPresentationTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-raids-test
    SOURCES
        test/RaidsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-redstone-tick-system-test
    SOURCES
        test/RedstoneTickSystemTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-redstone-tick-system-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/scratch")

basalt_test(server-runtime-environment-tick-system-test
    SOURCES
        test/EnvironmentTickSystemTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-environment-tick-system-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/scratch")

basalt_test(server-runtime-mob-tick-system-test
    SOURCES
        test/MobTickSystemTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-mob-tick-system-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/scratch")

basalt_test(server-runtime-mob-action-system-test
    SOURCES
        test/MobActionSystemTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-mob-action-system-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/scratch")

basalt_test(server-runtime-raid-system-test
    SOURCES
        test/RaidSystemTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-raid-system-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/scratch")

basalt_test(server-runtime-natural-spawning-system-test
    SOURCES
        test/NaturalSpawningSystemTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-natural-spawning-system-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/scratch")

basalt_test(server-runtime-combat-system-test
    SOURCES
        test/CombatSystemTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-combat-system-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/scratch")

basalt_test(server-runtime-explosion-system-test
    SOURCES
        test/ExplosionSystemTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-explosion-system-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/scratch")

basalt_test(server-runtime-block-change-broadcaster-test
    SOURCES
        test/BlockChangeBroadcasterTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-block-change-broadcaster-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/scratch")

basalt_test(server-runtime-status-effect-system-test
    SOURCES
        test/StatusEffectSystemTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-status-effect-system-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/scratch")

basalt_test(server-runtime-persistence-coordinator-test
    SOURCES
        test/PersistenceCoordinatorTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-persistence-coordinator-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/scratch")

basalt_test(server-runtime-vanilla-game-data-test
    SOURCES
        test/VanillaGameDataTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-server-facts-test
    SOURCES
        test/ServerFactsTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-server-facts-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/scratch")

basalt_test(server-runtime-vanilla-save-import-test
    SOURCES
        test/VanillaSaveImportTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-vanilla-save-import-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/scratch")

basalt_test(server-runtime-remaining-commands-test
    SOURCES
        test/RemainingCommandsTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-remaining-commands-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/scratch")

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

basalt_test(server-runtime-scoreboard-command-test
    SOURCES
        test/ScoreboardCommandTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-scoreboard-command-test
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

basalt_test(server-runtime-tag-command-test
    SOURCES
        test/TagCommandTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-tag-command-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-team-command-test
    SOURCES
        test/TeamCommandTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-team-command-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-teleport-command-test
    SOURCES
        test/TeleportCommandTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-teleport-command-test
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

basalt_test(server-runtime-timings-command-test
    SOURCES
        test/TimingsCommandTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-timings-command-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-time-command-test
    SOURCES
        test/TimeCommandTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-time-command-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-world-border-command-test
    SOURCES
        test/WorldBorderCommandTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-world-border-command-test
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

basalt_test(server-runtime-item-commands-test
    SOURCES
        test/ItemCommandsTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-item-commands-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-locate-command-test
    SOURCES
        test/LocateCommandTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-locate-command-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-particle-command-test
    SOURCES
        test/ParticleCommandTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-particle-command-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-data"
)

basalt_test(server-runtime-animation-placeholders-test
    SOURCES
        test/AnimationPlaceholdersTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-player-ui-test
    SOURCES
        test/PlayerUiTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-player-placeholders-test
    SOURCES
        test/PlayerPlaceholdersTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-server-placeholders-test
    SOURCES
        test/ServerPlaceholdersTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-placeholder-command-test
    SOURCES
        test/PlaceholderCommandTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-placeholder-command-test
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

basalt_test(server-runtime-player-movement-stats-test
    SOURCES
        test/PlayerMovementStatsTest.cpp
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

basalt_test(server-runtime-experience-orbs-test
    SOURCES
        test/ExperienceOrbsTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-experience-orbs-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/scratch")

basalt_test(server-runtime-end-fight-test
    SOURCES
        test/EndFightTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-end-fight-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/scratch")

basalt_test(server-runtime-shield-test
    SOURCES
        test/ShieldTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-sculk-shrieker-test
    SOURCES
        test/SculkShriekerTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-warden-brain-test
    SOURCES
        test/WardenBrainTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-mob-status-effects-test
    SOURCES
        test/MobStatusEffectsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-mob-conversions-test
    SOURCES
        test/MobConversionsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-mob-breathing-test
    SOURCES
        test/MobBreathingTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-swim-path-test
    SOURCES
        test/SwimPathTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-wither-test
    SOURCES
        test/WitherTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-fishing-test
    SOURCES
        test/FishingTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-ender-dragon-test
    SOURCES
        test/EnderDragonTest.cpp
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

basalt_test(server-runtime-random-sequences-test
    SOURCES
        test/RandomSequencesTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-random-sequences-test
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

basalt_test(server-runtime-beacons-test
    SOURCES
        test/BeaconsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-team-name-tags-test
    SOURCES
        test/TeamNameTagsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-books-test
    SOURCES
        test/BooksTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-looms-test
    SOURCES
        test/LoomsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-anvil-enchantments-test
    SOURCES
        test/AnvilEnchantmentsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-grindstones-test
    SOURCES
        test/GrindstonesTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-enchanting-test
    SOURCES
        test/EnchantingTest.cpp
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

basalt_test(server-runtime-arrow-buttons-test
    SOURCES
        test/ArrowButtonsTest.cpp
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

basalt_test(server-runtime-composter-test
    SOURCES
        test/ComposterTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-command-blocks-test
    SOURCES
        test/CommandBlocksTest.cpp
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

basalt_test(server-runtime-dripleaves-test
    SOURCES
        test/DripleavesTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-tripwires-test
    SOURCES
        test/TripwiresTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-wind-bursts-test
    SOURCES
        test/WindBurstsTest.cpp
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
