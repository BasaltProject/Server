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
        src/bedrock/BedrockMaps.cpp
        src/bedrock/BedrockPlayer.cpp
        src/bedrock/BedrockSeen.cpp
        src/bedrock/BedrockShield.cpp
        src/bedrock/BedrockPlayerConnection.cpp
        src/bedrock/BedrockRecipes.cpp
        src/bedrock/BedrockRecords.cpp
        src/bedrock/BedrockStackRequests.cpp
        src/bedrock/BedrockWindows.cpp
        src/config/AnimationsConfig.cpp
        src/config/BasaltConfig.cpp
        src/config/CommandsConfig.cpp
        src/config/CompatibilityConfig.cpp
        src/bukkit/BukkitRuntimeFiles.cpp
        src/bukkit/JavaPluginScan.cpp
        src/config/BukkitConfig.cpp
        src/config/ConfigDirectory.cpp
        src/config/ForwardingConfig.cpp
        src/config/GroupsConfig.cpp
        src/config/LiveConfigs.cpp
        src/config/ProfileSelection.cpp
        src/config/ServerListConfig.cpp
        src/config/UiConfigs.cpp
        src/entity/Aabb.cpp
        src/entity/BlockClip.cpp
        src/entity/BlockRay.cpp
        src/entity/BlockTriggers.cpp
        src/entity/EntityPhysics.cpp
        src/entity/FlyingMovement.cpp
        src/entity/FlyingPathFinder.cpp
        src/entity/PathTypes.cpp
        src/entity/Shulkers.cpp
        src/entity/LivingMovement.cpp
        src/entity/PathFinder.cpp
        src/entity/WalkNodeEvaluator.cpp
        src/entity/Minecarts.cpp
        src/entity/MinecartTrack.cpp
        src/entity/VehiclePresentation.cpp
        src/entity/PrimedTnt.cpp
        src/entity/Projectiles.cpp
        src/entity/EntityNbt.cpp
        src/entity/EntityCollision.cpp
        src/entity/MobRules.cpp
        src/entity/MobSounds.cpp
        src/entity/GeneratedEntityPresentation.cpp
        src/entity/Villagers.cpp
        src/entity/Raids.cpp
        src/entity/RaiderEquipment.cpp
        src/entity/FireworkRockets.cpp
        src/entity/IntHashOrder.cpp
        src/entity/WanderingTraderLifetime.cpp
        src/entity/ServerEntity.cpp
        src/entity/ShieldBlows.cpp
        src/config/WorldsConfig.cpp
        src/command/StringReader.cpp
        src/command/ArgumentTypes.cpp
        src/command/MessageArguments.cpp
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
        src/command/commands/FunctionRunner.cpp
        src/command/commands/DataPackCommand.cpp
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
        src/command/commands/PostEffectCommand.cpp
        src/command/commands/ComputeCommand.cpp
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
        src/command/DataArguments.cpp
        src/command/EntitySelector.cpp
        src/command/SelectorParser.cpp
        src/command/SelectorOptions.cpp
        src/BlockChangeBroadcaster.cpp
        src/BasaltServer.cpp
        src/BasaltServerBukkit.cpp
        src/BasaltServerConnections.cpp
        src/BasaltServerLife.cpp
        src/BasaltServerMelee.cpp
        src/BasaltServerData.cpp
        src/BasaltServerTick.cpp
        src/BasaltServerScores.cpp
        src/BlockInteractionHandler.cpp
        src/BlockUseHelpers.cpp
        src/BlockUseOn.cpp
        src/plugins/EventHooks.cpp
        src/plugins/EventSink.cpp
        src/plugins/WorldEventHooks.cpp
        src/plugins/PacketNames.cpp
        src/plugins/PacketNameTable.cpp
        src/plugins/PluginsCommand.cpp
        src/plugins/ServerNpcs.cpp
        src/plugins/ServerPluginServices.cpp
        src/plugins/ServerObjectModel.cpp
        src/plugins/ServerWorldModel.cpp
        src/ChunkSimulationView.cpp
        src/ClientSession.cpp
        src/ClientSessionPackets.cpp
        src/CommandWiring.cpp
        src/SessionLogin.cpp
        src/SessionPlay.cpp
        src/SessionChat.cpp
        src/chat/ChatBroadcast.cpp
        src/chat/ChatMessage.cpp
        src/chat/LastSeenValidator.cpp
        src/chat/SecureChat.cpp
        src/SessionDelegates.cpp
        src/ChunkStreamer.cpp
        src/CombatArrows.cpp
        src/CombatAttack.cpp
        src/CombatFireballs.cpp
        src/CombatSpit.cpp
        src/CombatHandler.cpp
        src/BlockTests.cpp
        src/CombatSystem.cpp
        src/CombatWindCharges.cpp
        src/ContainerClicks.cpp
        src/ContainerMenuController.cpp
        src/ContainerAnvil.cpp
        src/ContainerBeacon.cpp
        src/ContainerLoom.cpp
        src/ContainerCartography.cpp
        src/ContainerBrewing.cpp
        src/ContainerGrindstone.cpp
        src/ContainerEnchantment.cpp
        src/ContainerSlots.cpp
        src/ContainerRecipeAwards.cpp
        src/ContainerRecipePlacing.cpp
        src/MobActionSystem.cpp
        src/ItemUseTimers.cpp
        src/PlayerLifecycle.cpp
        src/VillagerTradeController.cpp
        src/MerchantSlots.cpp
        src/UiStateSync.cpp
        src/RedstoneTickSystem.cpp
        src/BlockEventTick.cpp
        src/ScheduledBlockTick.cpp
        src/MobTickSystem.cpp
        src/MobActionTick.cpp
        src/MobActionConversion.cpp
        src/MobActionRavager.cpp
        src/MobActionWarden.cpp
        src/MobDeathLoot.cpp
        src/MobActionAllay.cpp
        src/MobActionHorses.cpp
        src/MobActionCreaking.cpp
        src/MobActionCreakingHeart.cpp
        src/MobActionAmbient.cpp
        src/MobActionArmadillo.cpp
        src/MobActionAxolotl.cpp
        src/MobActionBees.cpp
        src/MobActionBreeze.cpp
        src/MobActionBreezeJump.cpp
        src/MobActionCamel.cpp
        src/MobActionFrog.cpp
        src/MobActionGoat.cpp
        src/MobActionHoglin.cpp
        src/MobActionNeutral.cpp
        src/MobActionPiglin.cpp
        src/MobActionSniffer.cpp
        src/MobActionPiglinItems.cpp
        src/MobActionCopperGolem.cpp
        src/MobActionShulker.cpp
        src/MobActionLlama.cpp
        src/MobActionGifts.cpp
        src/MobActionDolphin.cpp
        src/MobActionPufferfish.cpp
        src/MobActionWardenHearing.cpp
        src/BasaltServerFeatures.cpp
        src/EntityPushSystem.cpp
        src/SculkCatalystSystem.cpp
        src/ConduitSystem.cpp
        src/VehicleBoats.cpp
        src/VehicleMinecarts.cpp
        src/VehicleSystem.cpp
        src/ShriekerSystem.cpp
        src/LightningBoltSystem.cpp
        src/NaturalSpawningMaterialize.cpp
        src/NaturalSpawningSystem.cpp
        src/MobSpawnerSystem.cpp
        src/MobSpawnerSpawn.cpp
        src/SpawnerBlockTick.cpp
        src/world/BaseSpawnerData.cpp
        src/world/EquipmentTables.cpp
        src/world/MobSpawnChecks.cpp
        src/world/SpawnPlacementRules.cpp
        src/RaidSystem.cpp
        src/FireworkSystem.cpp
        src/RaidMembers.cpp
        src/RaidWaves.cpp
        src/MobActionVillager.cpp
        src/VillagerWorldSystem.cpp
        src/EntityVisibilityExtras.cpp
        src/EntityVisibilityTracker.cpp
        src/EnvironmentTickSystem.cpp
        src/ExplosionSystem.cpp
        src/EndFightSystem.cpp
        src/ExperienceOrbSystem.cpp
        src/FishingSystem.cpp
        src/MapSystem.cpp
        src/MapExplorer.cpp
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
        src/datapack/DataFiles.cpp
        src/datapack/DataPacks.cpp
        src/datapack/DataPackSelection.cpp
        src/datapack/PackFiles.cpp
        src/datapack/PackMetadata.cpp
        src/datapack/PackRepository.cpp
        src/datapack/ResourceStack.cpp
        src/datapack/RegistryValidation.cpp
        src/datapack/RegistrySchemas.cpp
        src/datapack/ReloadableElements.cpp
        src/player/FoodData.cpp
        src/player/PlayerArrowPickup.cpp
        src/player/PlayerMovementStats.cpp
        src/player/PlayerSession.cpp
        src/player/PlayerEntityData.cpp
        src/player/PlayerImpulse.cpp
        src/player/PlayerPersistence.cpp
        src/player/PlayerBlockUpdates.cpp
        src/player/PlayerDeath.cpp
        src/player/PlayerBeehives.cpp
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
        src/player/PlayerVersusPlayer.cpp
        src/player/PlayerMace.cpp
        src/player/PlayerThrown.cpp
        src/player/PlayerSpear.cpp
        src/player/PlayerTrident.cpp
        src/player/PlayerVelocity.cpp
        src/player/PlayerGliding.cpp
        src/player/PlayerFireworks.cpp
        src/player/PlayerCrossbow.cpp
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
        src/player/StatAwards.cpp
        src/player/PlayerCakesAndPots.cpp
        src/player/PlayerCauldrons.cpp
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
        src/registry/SyncedTags.cpp
        src/registry/PackRegistries.cpp
        src/sched/Owner.cpp
        src/sched/RegionMap.cpp
        src/sched/RegionScheduler.cpp
        src/sched/RegionWorkers.cpp
        src/scoreboard/Scoreboard.cpp
        src/scoreboard/ScoreboardStorage.cpp
        src/scoreboard/TeamNameTags.cpp
        src/scoreboard/Teams.cpp
        src/ui/PlayerUi.cpp
        src/timings/TimingsReport.cpp
        src/world/ChorusFlowerGrowth.cpp
        src/world/Paintings.cpp
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
        src/world/RealPotionTags.cpp
        src/world/RealDamageTypeTags.cpp
        src/world/DialogNbt.cpp
        src/world/Dialogs.cpp
        src/world/RealDialogs.cpp
        src/world/RealTestEnvironments.cpp
        src/world/RealTestInstances.cpp
        src/world/BlockInteractions.cpp
        src/world/ProjectileHits.cpp
        src/world/Furnaces.cpp
        src/world/ArrowButtons.cpp
        src/world/Hoppers.cpp
        src/world/EndPortals.cpp
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
        src/world/BlockTransformers.cpp
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
        src/world/PotionEffects.cpp
        src/world/Walls.cpp
        src/world/maps/MapData.cpp
        src/world/maps/Cartography.cpp
        src/world/maps/MapDecorationTypes.cpp
        src/world/maps/MapRenderer.cpp
        src/world/maps/MapStorage.cpp
        src/world/Waxing.cpp
        src/world/CommandPredicates.cpp
        src/world/EnderEyes.cpp
        src/world/AdvancementTree.cpp
        src/world/PlayerAdvancements.cpp
        src/world/VanillaAdvancementImport.cpp
        src/world/VariantSpawnConditions.cpp
        src/world/SpawnPlaces.cpp
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
        src/world/TriggerFacts.cpp
        src/world/BlockPickData.cpp
        src/player/PlayerTotem.cpp
        src/player/PlayerAttributes.cpp
        src/CombatVehicles.cpp
        src/CombatMounts.cpp
        src/CombatShearing.cpp
        src/CombatSpear.cpp
        src/CombatSpin.cpp
        src/CombatTridents.cpp
        src/StructureBlockSystem.cpp
        src/ContainerEntities.cpp
        src/world/ContainerDrops.cpp
        src/entity/Boats.cpp
        src/entity/Horses.cpp
        src/entity/CreakingNavigation.cpp
        src/entity/Creakings.cpp
        src/world/StructureBlocks.cpp
        src/entity/HorseBreeding.cpp
        src/entity/BoatPhysics.cpp
        src/world/Anvils.cpp
        src/world/Beacons.cpp
        src/world/Beehives.cpp
        src/world/Books.cpp
        src/world/AnvilEnchantments.cpp
        src/world/EnchantmentBooks.cpp
        src/world/Enchanting.cpp
        src/world/Grindstones.cpp
        src/world/Looms.cpp
        src/world/DroppedItemPhysics.cpp
        src/world/EntityRegistry.cpp
        src/world/FluidCurrents.cpp
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
        src/world/Conduits.cpp
        src/world/StalactiteDrips.cpp
        src/world/Crafters.cpp
        src/world/Cushions.cpp
        src/world/AttachedEntities.cpp
        src/world/FallLanding.cpp
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
        src/world/SavedDataFiles.cpp
        src/world/RandomTicks.cpp
        src/world/Reshapes.cpp
        src/world/RecipeBook.cpp
        src/world/Redstone.cpp
        src/world/RedstoneComponents.cpp
        src/world/RedstoneReactors.cpp
        src/world/ScheduledBlocks.cpp
        src/world/SculkSpreading.cpp
        src/world/ThrownItemTriggers.cpp
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
        src/world/OldTerrainBlending.cpp
        src/world/WorldMetadata.cpp
        src/world/WorldMetadataVanilla.cpp
        src/world/WorldSpawnPoint.cpp
        src/world/CustomSpawners.cpp
        src/world/MobSpawnAttributes.cpp
        src/world/MobSpawnFinalization.cpp
        src/world/MobSpawnFinalizers.cpp
        src/world/MobSpawnForest.cpp
        src/world/MobVariantSpawnFinalizer.cpp
        src/world/NaturalSpawner.cpp
        src/world/NetherPortals.cpp
        src/world/OriginalMobSpawner.cpp
        src/world/MobSpawnVariantData.cpp
        src/world/SpawnEquipmentEnchantments.cpp
        src/world/SpawnEquipmentFinalizer.cpp
        src/world/SkeletonHorseTrap.cpp
        src/world/Precipitation.cpp
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
        src/entity/Allays.cpp
        src/entity/Bees.cpp
        src/entity/MobEquipment.cpp
        src/entity/NeutralAnger.cpp
        src/entity/Piglins.cpp
        src/entity/CopperGolem.cpp
        src/entity/EntityCombat.cpp
        src/entity/MobBrain.cpp
        src/entity/MobCombat.cpp
        src/entity/ai/Controls.cpp
        src/entity/ai/FlyingGoals.cpp
        src/entity/ai/HoverWanderGoal.cpp
        src/entity/ai/HorseGoals.cpp
        src/entity/ai/LlamaGoals.cpp
        src/entity/ai/PhantomGoals.cpp
        src/entity/ai/ShulkerGoals.cpp
        src/entity/ai/WardenGoals.cpp
        src/entity/ai/GoalSelector.cpp
        src/entity/ai/Goals.cpp
        src/entity/ai/IllagerGoals.cpp
        src/entity/ai/RaiderGoals.cpp
        src/entity/ai/LongDistancePatrolGoal.cpp
        src/entity/ai/MobContext.cpp
        src/entity/ai/MobGoals.cpp
        src/entity/ai/SpecialGoals.cpp
        src/entity/ai/TameGoals.cpp
        src/entity/ai/TargetGoals.cpp
        src/entity/ai/RandomPos.cpp
        src/entity/villager/BrainTables.cpp
        src/entity/villager/CoreBehaviors.cpp
        src/entity/villager/Gossips.cpp
        src/entity/villager/PoiBehaviors.cpp
        src/entity/villager/RaidBehaviors.cpp
        src/entity/villager/SocialBehaviors.cpp
        src/entity/villager/VillagerBrain.cpp
        src/entity/villager/VillagerHelpers.cpp
        src/entity/villager/VillagerLife.cpp
        src/entity/villager/VillagerTrading.cpp
        src/entity/villager/VillagerNbt.cpp
        src/entity/villager/VillagerSensors.cpp
        src/entity/villager/WorkBehaviors.cpp
        src/entity/ai/VillagerSchedule.cpp
        src/entity/ai/WanderingTraderGoals.cpp
        src/entity/ai/WitherGoals.cpp
        src/entity/ai/DolphinGoals.cpp
        src/entity/ai/DolphinItemGoals.cpp
        src/entity/ai/CatGoals.cpp
        src/entity/ai/CopperGolemGoals.cpp
        src/entity/ai/BeeGoals.cpp
        src/entity/ai/BeeNavigation.cpp
        src/entity/ai/BrainKit.cpp
        src/entity/ai/LongJump.cpp
        src/entity/ai/NeutralGoals.cpp
        src/entity/ai/EndermanGoals.cpp
        src/entity/ai/ParrotGoals.cpp
        src/entity/Parrots.cpp
        src/MobActionParrots.cpp
        src/MobActionMovement.cpp
        src/MobActionRiderFalls.cpp
        src/entity/MobSteps.cpp
        src/entity/Leashes.cpp
        src/MobActionLeash.cpp
        src/entity/ai/DoorInteraction.cpp
        src/world/VillageDataLoader.cpp
        src/world/VillageGolemSpawner.cpp
        src/world/VillageProximity.cpp
        src/world/poi/PoiManager.cpp
        src/world/poi/PoiStorage.cpp
        src/world/poi/PoiTypes.cpp
        src/world/PostAttackEffects.cpp
        src/world/PostPiercingEffects.cpp
        src/world/WeaponDamage.cpp
        src/world/WorldState.cpp
        src/world/SpawnPotential.cpp
    DEPENDS basalt::server-assets basalt::wire-lua basalt::wire-bedrock basalt::server-plugins basalt::server-random basalt::server-registry basalt::server-runtime-config-defaults basalt::server-storage basalt::server-world basalt::wire-auth basalt::wire-io
            basalt::wire-multiversion basalt::wire-nbt basalt::wire-protocol basalt::wire-text
)

target_compile_definitions(server-runtime-core
    PUBLIC BASALT_VERSION="${BASALT_VERSION}"
)

if(BASALT_BUKKIT_ACTIVE)
    target_link_libraries(server-runtime-core PUBLIC basalt::server-bukkit)
    target_compile_definitions(server-runtime-core PUBLIC BASALT_WITH_BUKKIT=1)
endif()

basalt_test(server-runtime-entity-selector-test
    SOURCES
        test/EntitySelectorTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-dialog-inputs-test
    SOURCES
        test/DialogInputsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-entity-nbt-test
    SOURCES
        test/EntityNbtTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-biome-zoom-seed-test
    SOURCES
        test/BiomeZoomSeedTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-data-paths-test
    SOURCES
        test/DataPathsTest.cpp
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

basalt_test(server-runtime-block-change-broadcaster-test
    SOURCES
        test/BlockChangeBroadcasterTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-block-change-broadcaster-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/scratch")

basalt_test(server-runtime-dispenser-brush-test
    SOURCES
        test/DispenserBrushTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-dispenser-brush-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/scratch")

basalt_test(server-runtime-persistence-coordinator-test
    SOURCES
        test/PersistenceCoordinatorTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-persistence-coordinator-test
    PRIVATE BASALT_TEST_SCRATCH_DIR="${CMAKE_CURRENT_BINARY_DIR}/scratch")

basalt_test(server-runtime-secure-chat-test
    SOURCES
        test/SecureChatTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-biome-tag-loader-test
    SOURCES
        test/BiomeTagLoaderTest.cpp
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

basalt_test(server-runtime-item-component-codecs-test
    SOURCES
        test/ItemComponentCodecsTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-string-reader-test
    SOURCES
        test/StringReaderTest.cpp
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

basalt_test(server-runtime-map-data-test
    SOURCES
        test/MapDataTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-biome-registry-test
    SOURCES
        test/BiomeRegistryTest.cpp
    DEPENDS basalt::server-runtime-core
)

basalt_test(server-runtime-config-test
    SOURCES
        test/ConfigDirectoryTest.cpp
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

basalt_test(server-runtime-live-config-test
    SOURCES
        test/LiveConfigsTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-live-config-test
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

# The only test here that writes files, so it is told where inside the build tree to put them.

basalt_test(server-runtime-datapack-test
    SOURCES
        test/DataPackTest.cpp
    DEPENDS basalt::server-runtime-core
)

target_compile_definitions(server-runtime-datapack-test
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

if(UNIX)
    basalt_test(server-runtime-console-test
        SOURCES test/ConsoleReaderTest.cpp
        DEPENDS basalt::server-runtime-core
    )
endif()

basalt_test(server-runtime-player-persistence-test
    SOURCES test/PlayerPersistenceTest.cpp
    DEPENDS basalt::server-runtime-core
)
