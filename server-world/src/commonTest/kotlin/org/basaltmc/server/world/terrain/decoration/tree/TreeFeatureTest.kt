package org.basaltmc.server.world.terrain.decoration.tree

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.random.BitRandom
import org.basaltmc.server.random.PositionalRandomFactory
import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.terrain.decoration.IntProvider
import org.basaltmc.server.world.terrain.decoration.TreeDecoration
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class TreeFeatureTest {
    private fun chunkWithFlatGround(groundY: Int): Chunk {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                chunk.setBlockStateId(x, groundY, z, org.basaltmc.server.registry.Blocks.GRASS_BLOCK.stateId)
            }
        }
        return chunk
    }

    @Test
    fun oakPlacesLogsAndLeaves() {
        val chunk = chunkWithFlatGround(0)
        val random = WorldgenRandom(XoroshiroRandom(42L))
        val placed = TreeFeature.place(chunk, random, 8, 1, 8, TreeConfigs.OAK)
        assertTrue(placed, "expected the oak tree to place successfully")

        var logs = 0
        var leaves = 0
        for (x in 0 until 16) {
            for (y in 1..20) {
                for (z in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    if (id == oakLogState) logs++
                    if (isLeavesOf(id, oakLeavesState)) leaves++
                }
            }
        }
        assertTrue(logs > 0, "expected at least one oak log")
        assertTrue(leaves > 0, "expected at least one oak leaves block")
    }

    @Test
    fun treeConfigurationsMatchOfficialIgnoreVinesFlags() {
        val ignoreVines =
            listOf(
                TreeConfigs.OAK,
                TreeConfigs.BIRCH,
                TreeConfigs.SUPER_BIRCH,
                TreeConfigs.FANCY_OAK,
                TreeConfigs.JUNGLE_TREE,
                TreeConfigs.SPRUCE,
                TreeConfigs.PINE,
                TreeConfigs.ACACIA,
                TreeConfigs.DARK_OAK,
                TreeConfigs.PALE_OAK,
                TreeConfigs.PALE_OAK_CREAKING,
                TreeConfigs.MANGROVE,
                TreeConfigs.TALL_MANGROVE,
                TreeConfigs.CHERRY,
                TreeConfigs.OAK_LEAF_LITTER,
                TreeConfigs.FANCY_OAK_BEES,
            )
        val vinesBlockGrowth =
            listOf(
                TreeConfigs.AZALEA,
                TreeConfigs.SWAMP_OAK,
                TreeConfigs.JUNGLE_BUSH,
                TreeConfigs.MEGA_JUNGLE,
                TreeConfigs.MEGA_SPRUCE,
                TreeConfigs.MEGA_PINE,
            )

        assertTrue(ignoreVines.all(TreeConfiguration::ignoreVines))
        assertTrue(vinesBlockGrowth.none(TreeConfiguration::ignoreVines))
    }

    @Test
    fun vineOnlyBlocksTreesWhoseConfigurationDoesNotIgnoreIt() {
        val ignored = chunkWithFlatGround(0)
        val blocking = chunkWithFlatGround(0)
        val vine =
            org.basaltmc.server.registry.Blocks.REGISTRY
                .byIdentifier(Identifier.vanilla("vine"))
                ?.stateId ?: error("unknown vine")
        ignored.setBlockStateId(8, 2, 8, vine)
        blocking.setBlockStateId(8, 2, 8, vine)

        assertTrue(TreeFeature.place(ignored, WorldgenRandom(XoroshiroRandom(42L)), 8, 1, 8, TreeConfigs.OAK))
        assertFalse(TreeFeature.place(blocking, WorldgenRandom(XoroshiroRandom(42L)), 8, 1, 8, TreeConfigs.SWAMP_OAK))
    }

    @Test
    fun fancyOakAcceptsItsConfiguredMinimumClippedHeight() {
        val seed = 42L
        val sampledHeight =
            TreeConfigs.FANCY_OAK.trunkPlacer
                .getTreeHeight(WorldgenRandom(XoroshiroRandom(seed)))
        assertTrue(sampledHeight > 4)
        val chunk = chunkWithFlatGround(0)
        chunk.setBlockStateId(8, 7, 8, org.basaltmc.server.registry.Blocks.STONE.stateId)

        assertTrue(TreeFeature.place(chunk, WorldgenRandom(XoroshiroRandom(seed)), 8, 1, 8, TreeConfigs.FANCY_OAK))
    }

    @Test
    fun treePreflightAndTrunkReplaceVegetationFromTheVanillaTag() {
        val chunk = chunkWithFlatGround(0)
        val shortGrass =
            org.basaltmc.server.registry.Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.wire.common.Identifier
                        .vanilla("short_grass"),
                )?.stateId ?: error("unknown short_grass")
        chunk.setBlockStateId(8, 1, 8, shortGrass)

        assertTrue(TreeFeature.place(chunk, WorldgenRandom(XoroshiroRandom(42L)), 8, 1, 8, TreeConfigs.OAK))
        assertEquals(oakLogState, chunk.getBlockStateId(8, 1, 8))
    }

    @Test
    fun unchangedFoliageStillFeedsDecoratorContext() {
        val config =
            TreeConfiguration(
                oakLogState,
                StraightTrunkPlacer(1, 0, 0),
                oakLeavesState,
                BlobFoliagePlacer(IntProvider.of(2), IntProvider.of(0), 0),
                FeatureSizeTwoLayers(0, 0, 0),
                decorators = listOf(TreeDecoratorConfiguration.LeafVines(1.0f)),
                ignoreVines = true,
            )
        val chunk = chunkWithFlatGround(0)
        chunk.setBlockStateId(8, 0, 8, org.basaltmc.server.registry.Blocks.DIRT.stateId)
        for (x in 7..9) {
            for (z in 7..9) chunk.setBlockStateId(x, 2, z, oakLeavesState)
        }

        assertTrue(TreeFeature.place(chunk, WorldgenRandom(XoroshiroRandom(0L)), 8, 1, 8, config))
        val perimeter =
            buildList {
                for (offset in 6..10) {
                    add(6 to offset)
                    add(10 to offset)
                    add(offset to 6)
                    add(offset to 10)
                }
            }
        assertTrue(
            perimeter.any { (x, z) ->
                val stateId = chunk.getBlockStateId(x, 2, z)
                Blocks.isVineState(stateId)
            },
            "same-state foliage placements must remain visible to decorators",
        )
    }

    @Test
    fun treeWithNoPlacedLogsOrLeavesReportsFailure() {
        val chunk = chunkWithFlatGround(0)
        chunk.setBlockStateId(8, 0, 8, Blocks.DIRT.stateId)
        chunk.setBlockStateId(8, 1, 8, oakLogState)
        val emptyTree =
            TreeConfiguration(
                oakLogState,
                StraightTrunkPlacer(1, 0, 0),
                oakLeavesState,
                BlobFoliagePlacer(IntProvider.of(0), IntProvider.of(0), 0),
                FeatureSizeTwoLayers(0, 0, 0),
                ignoreVines = true,
            )

        assertFalse(TreeFeature.place(chunk, WorldgenRandom(XoroshiroRandom(0L)), 8, 1, 8, emptyTree))
    }

    @Test
    fun foliageOnlyWaterlogsInSourceWater() {
        val chunk = chunkWithFlatGround(0)
        chunk.setBlockStateId(8, 4, 8, org.basaltmc.server.registry.Blocks.WATER.stateId + 1)
        RandomSpreadFoliagePlacer(
            IntProvider.of(1),
            IntProvider.of(0),
            IntProvider.of(1),
            1,
        ).createFoliage(
            chunk,
            WorldgenRandom(XoroshiroRandom(1L)),
            oakLeavesState,
            FoliageAttachment(8, 4, 8, 0, false),
            1,
            1,
            0,
            mutableSetOf(),
        )

        assertEquals(
            "false",
            org.basaltmc.server.registry.Blocks
                .byStateId(chunk.getBlockStateId(8, 4, 8))
                ?.properties
                ?.get("waterlogged"),
        )
    }

    @Test
    fun foliageClearsProviderWaterloggingOutsideSourceWater() {
        val selected = Blocks.byStateId(oakLeavesState) ?: error("unknown oak leaves")
        val waterloggedLeaves =
            Blocks.stateIdOf(selected.block, selected.properties + ("waterlogged" to "true"))
                ?: error("unknown waterlogged oak leaves")
        val config =
            TreeConfiguration(
                oakLogState,
                StraightTrunkPlacer(1, 0, 0),
                waterloggedLeaves,
                BlobFoliagePlacer(IntProvider.of(2), IntProvider.of(0), 0),
                FeatureSizeTwoLayers(0, 0, 0),
                ignoreVines = true,
            )
        val chunk = chunkWithFlatGround(0)

        assertTrue(TreeFeature.place(chunk, WorldgenRandom(XoroshiroRandom(0L)), 8, 1, 8, config))
        assertEquals("false", Blocks.byStateId(chunk.getBlockStateId(8, 2, 8))?.properties?.get("waterlogged"))
    }

    @Test
    fun canopyCrossingAChunkBorderKeepsItsWorldCoordinates() {
        val chunk = chunkWithFlatGround(0)
        assertTrue(TreeFeature.place(chunk, WorldgenRandom(XoroshiroRandom(42L)), 15, 1, 8, TreeConfigs.OAK))

        val writes = chunk.takeGeneratedBlockWrites()
        assertTrue(writes.isNotEmpty(), "expected the edge canopy to spill into the eastern chunk")
        assertTrue(writes.all { it.x shr 4 != chunk.chunkX || it.z shr 4 != chunk.chunkZ })
        assertTrue(writes.any { isLeavesOf(it.stateId, oakLeavesState) })
        for (y in 1..20) {
            assertTrue(
                org.basaltmc.server.registry.Blocks
                    .isAir(chunk.getBlockStateId(0, y, 8)),
                "a world-x 16 leaf wrapped into local x=0 at y=$y",
            )
        }
    }

    @Test
    fun leaningTrunksCrossTheBorderInsteadOfTeleportingToTheOppositeEdge() {
        var externalLogs = emptyList<org.basaltmc.server.world.ChunkGeneratedBlockWrite>()
        for (seed in 0L..64L) {
            val chunk = chunkWithFlatGround(0)
            if (!TreeFeature.place(chunk, WorldgenRandom(XoroshiroRandom(seed)), 15, 1, 8, TreeConfigs.ACACIA)) continue
            val writes = chunk.takeGeneratedBlockWrites()
            externalLogs = writes.filter { it.stateId in acaciaLogState - 1..acaciaLogState + 1 }
            if (externalLogs.isNotEmpty()) {
                for (y in 1..20) {
                    assertTrue(chunk.getBlockStateId(0, y, 8) !in acaciaLogState - 1..acaciaLogState + 1)
                }
                break
            }
        }
        assertTrue(externalLogs.isNotEmpty(), "expected an acacia seed whose leaning trunk crosses east")
        assertTrue(externalLogs.all { it.x >= 16 })
    }

    @Test
    fun acaciaBranchBudgetIsConsumedBeforeTheBranchReachesItsMinimumHeight() {
        val random =
            WorldgenRandom(
                ScriptedBitRandom(
                    0,
                    1_610_612_736,
                    0,
                    536_870_912,
                    1_073_741_824,
                    0,
                ),
            )
        val attachments =
            ForkingTrunkPlacer(5, 0, 0).placeTrunk(
                chunkWithFlatGround(0),
                random,
                5,
                8,
                1,
                8,
                acaciaLogState,
            )

        assertEquals(1, attachments.size, "the one-step secondary branch is spent below y=1")
    }

    @Test
    fun oakAppliesTheDefaultBelowTrunkDirtProvider() {
        val grass = chunkWithFlatGround(0)
        assertTrue(TreeFeature.place(grass, WorldgenRandom(XoroshiroRandom(42L)), 8, 1, 8, TreeConfigs.OAK))
        assertTrue(grass.getBlockStateId(8, 0, 8) == org.basaltmc.server.registry.Blocks.DIRT.stateId)

        val podzol = chunkWithFlatGround(0)
        podzol.setBlockStateId(8, 0, 8, org.basaltmc.server.registry.Blocks.PODZOL.stateId)
        assertTrue(TreeFeature.place(podzol, WorldgenRandom(XoroshiroRandom(42L)), 8, 1, 8, TreeConfigs.OAK))
        assertTrue(podzol.getBlockStateId(8, 0, 8) == org.basaltmc.server.registry.Blocks.PODZOL.stateId)
    }

    @Test
    fun darkOakAppliesTheBelowTrunkProviderAcrossItsTwoByTwoFootprint() {
        val chunk = chunkWithFlatGround(0)
        assertTrue(TreeFeature.place(chunk, WorldgenRandom(XoroshiroRandom(31L)), 7, 1, 7, TreeConfigs.DARK_OAK))
        for (x in 7..8) {
            for (z in 7..8) {
                assertTrue(
                    chunk.getBlockStateId(x, 0, z) == org.basaltmc.server.registry.Blocks.DIRT.stateId,
                )
            }
        }
    }

    @Test
    fun fancyOakPlacesBranchedLogsAndRoundedFoliageClusters() {
        val chunk = chunkWithFlatGround(0)
        val random = WorldgenRandom(XoroshiroRandom(42L))
        val placed = TreeFeature.place(chunk, random, 8, 1, 8, TreeConfigs.FANCY_OAK)
        assertTrue(placed, "expected the fancy oak tree to place successfully")

        var logs = 0
        var leaves = 0
        for (x in 0 until 16) {
            for (y in 1..24) {
                for (z in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    if (id in oakLogState - 1..oakLogState + 1) logs++
                    if (isLeavesOf(id, oakLeavesState)) leaves++
                }
            }
        }
        assertTrue(logs > 4, "expected a trunk and limbs, got $logs logs")
        assertTrue(leaves > 0, "expected at least one rounded foliage cluster")
    }

    @Test
    fun nullRandomSelectorEntryConsumesTheRollWithoutPlacingTheDefault() {
        val chunk = chunkWithFlatGround(0)
        TreeDecoration
            .sequence(listOf(1.0f to null), TreeConfigs.OAK, 1)
            .decorate(chunk, WorldgenRandom(XoroshiroRandom(5L)))

        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 1..20) assertTrue(chunk.getBlockStateId(x, y, z) !in oakLogState - 1..oakLogState + 1)
            }
        }
    }

    @Test
    fun waterTreeWeightedCountCanPlaceAnOak() {
        val seed =
            (0L..1_000L).first { candidate ->
                WorldgenRandom(XoroshiroRandom(candidate)).nextInt(10) == 9
            }
        val chunk = chunkWithFlatGround(0)
        TreeDecoration.WATER.decorate(chunk, WorldgenRandom(XoroshiroRandom(seed)))

        var logs = 0
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 1..24) if (chunk.getBlockStateId(x, y, z) in oakLogState - 1..oakLogState + 1) logs++
            }
        }
        assertTrue(logs > 0, "expected the weighted 1/10 water-tree count to produce an oak")
    }

    @Test
    fun spruceWithPineFoliagePlacesLogsAndLeaves() {
        val chunk = chunkWithFlatGround(0)
        val random = WorldgenRandom(XoroshiroRandom(1234L))
        val placed = TreeFeature.place(chunk, random, 8, 1, 8, TreeConfigs.SPRUCE)
        assertTrue(placed, "expected the spruce tree to place successfully")

        var logs = 0
        var leaves = 0
        for (x in 0 until 16) {
            for (y in 1..24) {
                for (z in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    if (id == spruceLogState) logs++
                    if (isLeavesOf(id, spruceLeavesState)) leaves++
                }
            }
        }
        assertTrue(logs > 0, "expected at least one spruce log")
        assertTrue(leaves > 0, "expected at least one spruce leaves block")
    }

    @Test
    fun jungleBushPlacesOneLogWithOakLeafCanopy() {
        val chunk = chunkWithFlatGround(0)
        val placed = TreeFeature.place(chunk, WorldgenRandom(XoroshiroRandom(17L)), 8, 1, 8, TreeConfigs.JUNGLE_BUSH)
        assertTrue(placed)

        var logs = 0
        var leaves = 0
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 1..8) {
                    val state = chunk.getBlockStateId(x, y, z)
                    if (state == jungleLogState) logs++
                    if (isLeavesOf(state, oakLeavesState)) leaves++
                }
            }
        }
        assertTrue(logs == 1, "expected the bush's single jungle log, got $logs")
        assertTrue(leaves > 0)
    }

    @Test
    fun megaJunglePlacesGiantTrunkBranchesAndLeaves() {
        val chunk = chunkWithFlatGround(0)
        val placed = TreeFeature.place(chunk, WorldgenRandom(XoroshiroRandom(23L)), 7, 1, 7, TreeConfigs.MEGA_JUNGLE)
        assertTrue(placed)

        var logs = 0
        var leaves = 0
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 1..40) {
                    val state = chunk.getBlockStateId(x, y, z)
                    if (state == jungleLogState) logs++
                    if (isLeavesOf(state, jungleLeavesState)) leaves++
                }
            }
        }
        assertTrue(logs >= 31, "expected a giant trunk and branches, got $logs logs")
        assertTrue(leaves > 0)
    }

    @Test
    fun megaJungleAndSwampTreesReceiveTheirConfiguredVines() {
        for ((index, config) in listOf(TreeConfigs.MEGA_JUNGLE, TreeConfigs.SWAMP_OAK).withIndex()) {
            val chunk = chunkWithFlatGround(0)
            assertTrue(TreeFeature.place(chunk, WorldgenRandom(XoroshiroRandom(index + 71L)), 7, 1, 7, config))

            var vines = 0
            for (x in 0 until 16) {
                for (z in 0 until 16) {
                    for (y in 1..40) {
                        if (org.basaltmc.server.registry.Blocks.isVineState(
                                chunk.getBlockStateId(x, y, z),
                            )
                        ) {
                            vines++
                        }
                    }
                }
            }
            assertTrue(vines > 0, "expected decorated tree $index to place vines")
        }
    }

    @Test
    fun jungleTreesRunCocoaBeforeTheirTrunkAndLeafVines() {
        var cocoaBlocks = 0
        var vineBlocks = 0
        for (seed in 0L..63L) {
            val chunk = chunkWithFlatGround(0)
            assertTrue(
                TreeFeature.place(chunk, WorldgenRandom(XoroshiroRandom(seed)), 7, 1, 7, TreeConfigs.JUNGLE_TREE),
            )
            for (x in 0 until 16) {
                for (z in 0 until 16) {
                    for (y in 1..24) {
                        val stateId = chunk.getBlockStateId(x, y, z)
                        if (org.basaltmc.server.registry.Blocks
                                .isCocoaState(stateId)
                        ) {
                            cocoaBlocks++
                        }
                        if (org.basaltmc.server.registry.Blocks
                                .isVineState(stateId)
                        ) {
                            vineBlocks++
                        }
                    }
                }
            }
            if (cocoaBlocks > 0 && vineBlocks > 0) break
        }
        assertTrue(cocoaBlocks > 0, "expected the 0.2 cocoa decorator to place cocoa")
        assertTrue(vineBlocks > 0, "expected jungle trunk and leaf vines")
    }

    @Test
    fun darkOakPlacesA2x2TrunkAndLeaves() {
        val chunk = chunkWithFlatGround(0)
        val random = WorldgenRandom(XoroshiroRandom(7L))
        val placed = TreeFeature.place(chunk, random, 6, 1, 6, TreeConfigs.DARK_OAK)
        assertTrue(placed, "expected the dark oak tree to place successfully")

        var logs = 0
        var leaves = 0
        for (x in 0 until 16) {
            for (y in 1..16) {
                for (z in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    if (id == darkOakLogState) logs++
                    if (isLeavesOf(id, darkOakLeavesState)) leaves++
                }
            }
        }
        // A 2x2 trunk of at least baseHeight 6 must place at least 4*6 = 24 logs.
        assertTrue(logs >= 24, "expected at least 24 dark oak logs (2x2 trunk), got $logs")
        assertTrue(leaves > 0, "expected at least one dark oak leaves block")
    }

    @Test
    fun paleOakAddsItsGroundPatchAndHangingMoss() {
        var groundMoss = 0
        var hangingMoss = 0
        for (seed in 0L..15L) {
            val chunk = chunkWithFlatGround(0)
            assertTrue(TreeFeature.place(chunk, WorldgenRandom(XoroshiroRandom(seed)), 7, 1, 7, TreeConfigs.PALE_OAK))
            for (x in 0 until 16) {
                for (z in 0 until 16) {
                    for (y in 0..20) {
                        val stateId = chunk.getBlockStateId(x, y, z)
                        if (stateId == paleMossBlockState) groundMoss++
                        if (org.basaltmc.server.registry.Blocks
                                .isPaleHangingMossState(stateId)
                        ) {
                            hangingMoss++
                        }
                    }
                }
            }
            if (groundMoss > 0 && hangingMoss > 0) break
        }
        assertTrue(groundMoss > 0, "expected pale moss ground replacement")
        assertTrue(hangingMoss > 0, "expected pale hanging moss on the tree")
    }

    @Test
    fun paleOakGroundCarpetsUseTheLevelRandomStream() {
        var levelRandomCalls = 0
        for (seed in 0L..255L) {
            val chunk = chunkWithFlatGround(0)
            for (x in 11 until 16) {
                for (z in 0 until 16) chunk.setBlockStateId(x, 1, z, org.basaltmc.server.registry.Blocks.STONE.stateId)
            }
            val levelRandom = CountingRandom(XoroshiroRandom(seed xor 0x5EEDL))
            if (!TreeFeature.place(
                    chunk,
                    WorldgenRandom(XoroshiroRandom(seed)),
                    7,
                    1,
                    7,
                    TreeConfigs.PALE_OAK,
                    levelRandom,
                )
            ) {
                continue
            }
            levelRandomCalls += levelRandom.booleanCalls
            if (levelRandomCalls > 0) break
        }

        assertTrue(levelRandomCalls > 0, "expected nested MossyCarpetBlock placement to consume level random")
    }

    @Test
    fun creakingPaleOakCanReplaceASurroundedLogWithANaturalDormantHeart() {
        var hearts = 0
        var heartChunk: Chunk? = null
        for (seed in 0L..255L) {
            val chunk = chunkWithFlatGround(0)
            assertTrue(
                TreeFeature.place(chunk, WorldgenRandom(XoroshiroRandom(seed)), 7, 1, 7, TreeConfigs.PALE_OAK_CREAKING),
            )
            for (x in 0 until 16) {
                for (z in 0 until 16) {
                    for (y in 1..20) if (chunk.getBlockStateId(x, y, z) == naturalDormantCreakingHeartState) hearts++
                }
            }
            if (hearts > 0) {
                heartChunk = chunk
                break
            }
        }
        assertTrue(hearts > 0, "expected at least one generated dark-oak shape to contain a valid heart position")
        assertEquals(
            Identifier.vanilla("creaking_heart"),
            heartChunk
                ?.blockEntities()
                ?.single()
                ?.type,
        )
    }

    @Test
    fun creakingHeartBlockEntityKeepsItsWorldPositionAcrossAChunkBorder() {
        var externalHearts = emptyList<org.basaltmc.server.world.ChunkGeneratedBlockEntityWrite>()
        for (seed in 0L..255L) {
            val chunk = chunkWithFlatGround(0)
            if (!TreeFeature.place(
                    chunk,
                    WorldgenRandom(XoroshiroRandom(seed)),
                    15,
                    1,
                    7,
                    TreeConfigs.PALE_OAK_CREAKING,
                )
            ) {
                continue
            }
            externalHearts = chunk.takeGeneratedBlockEntityWrites()
            if (externalHearts.isNotEmpty()) break
        }

        val heart = externalHearts.single()
        assertTrue(heart.x >= 16)
        assertEquals(naturalDormantCreakingHeartState, heart.expectedStateId)
        assertEquals(Identifier.vanilla("creaking_heart"), heart.type)
    }

    @Test
    fun megaSprucePlacesA2x2TrunkAndACrownOfLeaves() {
        val chunk = chunkWithFlatGround(0)
        val random = WorldgenRandom(XoroshiroRandom(3L))
        val placed = TreeFeature.place(chunk, random, 6, 1, 6, TreeConfigs.MEGA_SPRUCE)
        assertTrue(placed, "expected the mega spruce tree to place successfully")

        var logs = 0
        var leaves = 0
        for (x in 0 until 16) {
            for (y in 1..30) {
                for (z in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    if (id == spruceLogState) logs++
                    if (isLeavesOf(id, spruceLeavesState)) leaves++
                }
            }
        }
        // baseHeight 13 minimum, 2x2 trunk for all but the topmost layer: at least 13*3+1 = 40 logs.
        assertTrue(logs >= 40, "expected at least 40 spruce logs (2x2 giant trunk), got $logs")
        assertTrue(leaves > 0, "expected at least one spruce leaves block")
    }

    @Test
    fun bothGiantTaigaTreesAlterReplaceableGroundToPodzol() {
        for ((index, config) in listOf(TreeConfigs.MEGA_SPRUCE, TreeConfigs.MEGA_PINE).withIndex()) {
            val chunk = chunkWithFlatGround(0)
            assertTrue(TreeFeature.place(chunk, WorldgenRandom(XoroshiroRandom(index + 31L)), 7, 1, 7, config))

            var podzol = 0
            for (x in 0 until 16) {
                for (z in 0 until 16) if (chunk.getBlockStateId(x, 0, z) == podzolState) podzol++
            }
            assertTrue(podzol > 0, "expected giant taiga tree $index to produce podzol circles")
        }
    }

    @Test
    fun giantTaigaPodzolDecoratorReplacesSnowyGroundVariants() {
        val chunk = chunkWithFlatGround(0)
        val grass =
            org.basaltmc.server.registry.Blocks
                .byStateId(org.basaltmc.server.registry.Blocks.GRASS_BLOCK.stateId) ?: error("unknown grass")
        val snowyGrass =
            org.basaltmc.server.registry.Blocks
                .stateIdOf(grass.block, grass.properties + ("snowy" to "true")) ?: error("unknown snowy grass")
        for (x in 0 until 16) {
            for (z in 0 until 16) chunk.setBlockStateId(x, 0, z, snowyGrass)
        }

        assertTrue(TreeFeature.place(chunk, WorldgenRandom(XoroshiroRandom(31L)), 7, 1, 7, TreeConfigs.MEGA_SPRUCE))
        var podzol = 0
        for (x in 0 until 16) {
            for (z in 0 until 16) if (chunk.getBlockStateId(x, 0, z) == podzolState) podzol++
        }
        assertTrue(podzol > 4, "the decorator must replace snowy substrate beyond the four trunk dirt blocks")
    }

    @Test
    fun darkOakTrunkReplacesEveryLeafStateVariant() {
        val chunk = chunkWithFlatGround(0)
        val defaultLeaves =
            org.basaltmc.server.registry.Blocks
                .byStateId(oakLeavesState) ?: error("unknown oak leaves")
        val distanceOneLeaves =
            org.basaltmc.server.registry.Blocks
                .stateIdOf(defaultLeaves.block, defaultLeaves.properties + ("distance" to "1"))
                ?: error("unknown distance-one oak leaves")
        chunk.setBlockStateId(8, 1, 8, distanceOneLeaves)

        assertTrue(TreeFeature.place(chunk, WorldgenRandom(XoroshiroRandom(31L)), 8, 1, 8, TreeConfigs.DARK_OAK))
        assertEquals(
            "dark_oak_log",
            org.basaltmc.server.registry.Blocks
                .byStateId(chunk.getBlockStateId(8, 1, 8))
                ?.block
                ?.path,
        )
    }

    @Test
    fun cherryPlacesLogsAndLeaves() {
        val chunk = chunkWithFlatGround(0)
        val random = WorldgenRandom(XoroshiroRandom(99L))
        val placed = TreeFeature.place(chunk, random, 8, 1, 8, TreeConfigs.CHERRY)
        assertTrue(placed, "expected the cherry tree to place successfully")

        var logs = 0
        var leaves = 0
        var horizontalLogs = 0
        for (x in 0 until 16) {
            for (y in 1..24) {
                for (z in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    val state =
                        org.basaltmc.server.registry.Blocks
                            .byStateId(id)
                    if (state?.block ==
                        org.basaltmc.wire.common.Identifier
                            .vanilla("cherry_log")
                    ) {
                        logs++
                        if (state.properties["axis"] in setOf("x", "z")) horizontalLogs++
                    }
                    if (isLeavesOf(id, cherryLeavesState)) leaves++
                }
            }
        }
        // baseHeight 7 plus at least one side branch: well above a plain straight trunk's 7 logs.
        assertTrue(logs >= 8, "expected at least 8 cherry logs (trunk plus a branch), got $logs")
        assertTrue(horizontalLogs > 0, "expected cherry's lateral branch logs to carry their horizontal axis")
        assertTrue(leaves > 0, "expected at least one cherry leaves block")
    }

    @Test
    fun cherryHangingLeavesOnlyExtendLeavesSetByTheCurrentTree() {
        val chunk = chunkWithFlatGround(0)
        val originX = 8
        val originY = 10
        val originZ = 8
        chunk.setBlockStateId(originX + 3, originY - 1, originZ + 2, cherryLeavesState)
        val placer =
            CherryFoliagePlacer(
                org.basaltmc.server.world.terrain.decoration.IntProvider
                    .of(4),
                org.basaltmc.server.world.terrain.decoration.IntProvider
                    .of(0),
                org.basaltmc.server.world.terrain.decoration.IntProvider
                    .of(5),
                1.0f,
                1.0f,
                1.0f,
                1.0f,
            )
        val foliagePositions = mutableSetOf<Triple<Int, Int, Int>>()

        placer.createFoliage(
            chunk,
            WorldgenRandom(XoroshiroRandom(7L)),
            cherryLeavesState,
            FoliageAttachment(originX, originY, originZ, 0, false),
            5,
            4,
            0,
            foliagePositions,
        )

        assertTrue(
            org.basaltmc.server.registry.Blocks
                .isAir(chunk.getBlockStateId(originX + 3, originY - 2, originZ + 2)),
            "an unrelated old canopy leaf must not sprout a new hanging extension",
        )
    }

    @Test
    fun mangrovePlacesLogsAndScatteredLeaves() {
        val chunk = chunkWithFlatGround(0)
        val random = WorldgenRandom(XoroshiroRandom(11L))
        val placed = TreeFeature.place(chunk, random, 8, 1, 8, TreeConfigs.MANGROVE)
        assertTrue(placed, "expected the mangrove tree to place successfully")

        var logs = 0
        var leaves = 0
        var roots = 0
        for (x in 0 until 16) {
            for (y in 1..20) {
                for (z in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    if (id == mangroveLogState) logs++
                    if (isLeavesOf(id, mangroveLeavesState)) leaves++
                    if (id == mangroveRootsState) roots++
                }
            }
        }
        assertTrue(logs > 0, "expected at least one mangrove log")
        assertTrue(leaves > 0, "expected at least one mangrove leaves block")
        assertTrue(roots > 0, "expected at least one mangrove roots block (the trunk starts elevated)")
    }

    @Test
    fun mangroveRootsAndLeavesKeepWaterAtFloodedPositions() {
        val chunk = chunkWithFlatGround(0)
        for (x in 3..13) {
            for (z in 3..13) {
                for (y in 1..20) chunk.setBlockStateId(x, y, z, org.basaltmc.server.registry.Blocks.WATER.stateId)
            }
        }

        assertTrue(TreeFeature.place(chunk, WorldgenRandom(XoroshiroRandom(11L)), 8, 1, 8, TreeConfigs.MANGROVE))

        var waterloggedRoots = 0
        var waterloggedLeaves = 0
        for (x in 0 until 16) {
            for (y in 1..20) {
                for (z in 0 until 16) {
                    val state =
                        org.basaltmc.server.registry.Blocks
                            .byStateId(
                                chunk.getBlockStateId(x, y, z),
                            ) ?: continue
                    if (state.block.path == "mangrove_roots" && state.properties["waterlogged"] == "true") {
                        waterloggedRoots++
                    }
                    if (state.block.path == "mangrove_leaves" && state.properties["waterlogged"] == "true") {
                        waterloggedLeaves++
                    }
                }
            }
        }
        assertTrue(waterloggedRoots > 0, "expected submerged mangrove roots to retain water")
        assertTrue(waterloggedLeaves > 0, "expected submerged mangrove leaves to retain water")
    }

    @Test
    fun mangroveRootKeepsMuddyProviderForEveryInputStateVariant() {
        val chunk = chunkWithFlatGround(0)
        val muddyDefault =
            org.basaltmc.server.registry.Blocks
                .byStateId(muddyMangroveRootsState) ?: error("unknown muddy mangrove roots")
        val xAxisMuddy =
            org.basaltmc.server.registry.Blocks
                .stateIdOf(muddyDefault.block, muddyDefault.properties + ("axis" to "x"))
                ?: error("unknown x-axis muddy mangrove roots")
        chunk.setBlockStateId(8, 1, 8, xAxisMuddy)

        assertTrue(
            MangroveRootPlacer(
                org.basaltmc.server.world.terrain.decoration.IntProvider
                    .of(1),
            ).placeRoots(
                chunk,
                WorldgenRandom(XoroshiroRandom(11L)),
                8,
                1,
                8,
                2,
            ),
        )
        assertEquals(
            "muddy_mangrove_roots",
            org.basaltmc.server.registry.Blocks
                .byStateId(chunk.getBlockStateId(8, 1, 8))
                ?.block
                ?.path,
        )
    }

    @Test
    fun mangroveTrunkGrowsThroughItsConfiguredBlockTag() {
        val chunk = chunkWithFlatGround(0)
        val mossCarpet =
            org.basaltmc.server.registry.Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.wire.common.Identifier
                        .vanilla("moss_carpet"),
                )?.stateId ?: error("unknown moss_carpet")
        chunk.setBlockStateId(8, 1, 8, mossCarpet)
        val base = TreeConfigs.MANGROVE
        val rootless =
            TreeConfiguration(
                base.trunkStateId,
                base.trunkPlacer,
                base.foliageStateId,
                base.foliagePlacer,
                base.minimumSize,
            )

        assertTrue(TreeFeature.place(chunk, WorldgenRandom(XoroshiroRandom(11L)), 8, 1, 8, rootless))
        assertEquals(
            "mangrove_log",
            org.basaltmc.server.registry.Blocks
                .byStateId(chunk.getBlockStateId(8, 1, 8))
                ?.block
                ?.path,
        )
    }

    @Test
    fun mangroveCanHangAgedPropagulesFromLeaves() {
        var propaguleState: Int? = null
        for (seed in 0L..255L) {
            val chunk = chunkWithFlatGround(0)
            assertTrue(TreeFeature.place(chunk, WorldgenRandom(XoroshiroRandom(seed)), 8, 1, 8, TreeConfigs.MANGROVE))
            outer@ for (x in 0 until 16) {
                for (z in 0 until 16) {
                    for (y in 1..24) {
                        val stateId = chunk.getBlockStateId(x, y, z)
                        if (org.basaltmc.server.registry.Blocks
                                .isMangrovePropaguleState(stateId)
                        ) {
                            propaguleState = stateId
                            break@outer
                        }
                    }
                }
            }
            if (propaguleState != null) break
        }
        val state =
            org.basaltmc.server.registry.Blocks
                .byStateId(propaguleState ?: error("expected a hanging propagule"))
                ?: error("unknown propagule state")
        assertTrue(state.properties["hanging"] == "true")
        assertTrue(state.properties["stage"] == "0")
        assertTrue(state.properties["waterlogged"] == "false")
        assertTrue(state.properties["age"]?.toIntOrNull()?.let { it in 0..4 } == true)
    }

    @Test
    fun leafLitterTreePlacesPersistableFacingAndSegmentStatesOnSolidGround() {
        val chunk = chunkWithFlatGround(0)
        assertTrue(TreeFeature.place(chunk, WorldgenRandom(XoroshiroRandom(42L)), 8, 1, 8, TreeConfigs.OAK_LEAF_LITTER))

        val states = mutableListOf<org.basaltmc.server.registry.BlockState>()
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 1..4) {
                    val stateId = chunk.getBlockStateId(x, y, z)
                    if (org.basaltmc.server.registry.Blocks
                            .isLeafLitterState(stateId)
                    ) {
                        states += org.basaltmc.server.registry.Blocks
                            .byStateId(stateId) ?: error("unknown leaf litter state")
                    }
                }
            }
        }
        assertTrue(states.isNotEmpty(), "expected PlaceOnGroundDecorator to add leaf litter")
        assertTrue(states.all { it.properties["facing"] in setOf("north", "east", "south", "west") })
        assertTrue(
            states.all {
                it.properties["segment_amount"]?.toIntOrNull()?.let { amount ->
                    amount in 1..4
                } == true
            },
        )
    }

    @Test
    fun guaranteedBeeDecoratorPlacesASouthFacingEmptyNest() {
        var beeNest: org.basaltmc.server.registry.BlockState? = null
        var decoratedChunk: Chunk? = null
        for (seed in 0L..255L) {
            val chunk = chunkWithFlatGround(0)
            assertTrue(
                TreeFeature.place(chunk, WorldgenRandom(XoroshiroRandom(seed)), 8, 1, 8, TreeConfigs.FANCY_OAK_BEES),
            )
            outer@ for (x in 0 until 16) {
                for (z in 0 until 16) {
                    for (y in 1..24) {
                        val stateId = chunk.getBlockStateId(x, y, z)
                        if (org.basaltmc.server.registry.Blocks
                                .isBeeNestState(stateId)
                        ) {
                            beeNest =
                                org.basaltmc.server.registry.Blocks
                                    .byStateId(stateId)
                            break@outer
                        }
                    }
                }
            }
            if (beeNest != null) {
                decoratedChunk = chunk
                break
            }
        }
        val nest = beeNest ?: error("expected a generated bee nest")
        assertTrue(nest.properties["facing"] == "south")
        assertTrue(nest.properties["honey_level"] == "0")

        val blockEntity = decoratedChunk!!.blockEntities().single()
        assertEquals(
            org.basaltmc.wire.common.Identifier
                .vanilla("beehive"),
            blockEntity.type,
        )
        val bees = blockEntity.data.getList("bees") ?: error("expected stored bee occupants")
        assertTrue(bees.size in 2..3)
        for (entry in bees) {
            val occupant = entry as NbtCompound
            assertEquals("minecraft:bee", occupant.getCompound("entity_data").getString("id"))
            assertTrue(occupant.getInt("ticks_in_hive") in 0..598)
            assertEquals(600, occupant.getInt("min_ticks_in_hive"))
        }
    }

    @Test
    fun generatedLeavesReceiveTheirDistanceFromTheNearestLog() {
        val chunk = chunkWithFlatGround(0)
        assertTrue(TreeFeature.place(chunk, WorldgenRandom(XoroshiroRandom(42L)), 8, 1, 8, TreeConfigs.OAK))

        val distances = mutableListOf<Int>()
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 1..20) {
                    val stateId = chunk.getBlockStateId(x, y, z)
                    if (isLeavesOf(stateId, oakLeavesState)) {
                        distances +=
                            org.basaltmc.server.registry.Blocks
                                .leafDistance(stateId) ?: error("missing leaf distance")
                    }
                }
            }
        }
        assertTrue(distances.isNotEmpty())
        assertTrue(1 in distances)
        assertTrue(distances.all { it in 1..6 })
    }

    private fun isLeavesOf(
        stateId: Int,
        defaultStateId: Int,
    ): Boolean = stateId in defaultStateId - 27..defaultStateId

    private companion object {
        private class CountingRandom(
            private val delegate: BasaltRandom,
        ) : BasaltRandom by delegate {
            var booleanCalls: Int = 0

            override fun nextBoolean(): Boolean {
                booleanCalls++
                return delegate.nextBoolean()
            }
        }

        private class ScriptedBitRandom(
            vararg values: Int,
        ) : BitRandom {
            private val values = values.toMutableList()

            override fun next(bits: Int): Int = values.removeAt(0)

            override fun fork(): ScriptedBitRandom = this

            override fun forkPositional(): PositionalRandomFactory = error("not used")

            override fun setSeed(seed: Long) = Unit

            override fun nextGaussian(): Double = error("not used")
        }

        val oakLogState =
            org.basaltmc.server.registry.Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.wire.common.Identifier
                        .vanilla("oak_log"),
                )?.stateId ?: error("unknown block")
        val oakLeavesState =
            org.basaltmc.server.registry.Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.wire.common.Identifier
                        .vanilla("oak_leaves"),
                )?.stateId ?: error("unknown block")
        val spruceLogState =
            org.basaltmc.server.registry.Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.wire.common.Identifier
                        .vanilla("spruce_log"),
                )?.stateId ?: error("unknown block")
        val acaciaLogState =
            org.basaltmc.server.registry.Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.wire.common.Identifier
                        .vanilla("acacia_log"),
                )?.stateId ?: error("unknown block")
        val spruceLeavesState =
            org.basaltmc.server.registry.Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.wire.common.Identifier
                        .vanilla("spruce_leaves"),
                )?.stateId ?: error("unknown block")
        val darkOakLogState =
            org.basaltmc.server.registry.Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.wire.common.Identifier
                        .vanilla("dark_oak_log"),
                )?.stateId ?: error("unknown block")
        val darkOakLeavesState =
            org.basaltmc.server.registry.Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.wire.common.Identifier
                        .vanilla("dark_oak_leaves"),
                )?.stateId ?: error("unknown block")
        val mangroveLogState =
            org.basaltmc.server.registry.Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.wire.common.Identifier
                        .vanilla("mangrove_log"),
                )?.stateId ?: error("unknown block")
        val mangroveLeavesState =
            org.basaltmc.server.registry.Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.wire.common.Identifier
                        .vanilla("mangrove_leaves"),
                )?.stateId ?: error("unknown block")
        val cherryLogState =
            org.basaltmc.server.registry.Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.wire.common.Identifier
                        .vanilla("cherry_log"),
                )?.stateId ?: error("unknown block")
        val cherryLeavesState =
            org.basaltmc.server.registry.Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.wire.common.Identifier
                        .vanilla("cherry_leaves"),
                )?.stateId ?: error("unknown block")
        val jungleLogState =
            org.basaltmc.server.registry.Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.wire.common.Identifier
                        .vanilla("jungle_log"),
                )?.stateId ?: error("unknown block")
        val jungleLeavesState =
            org.basaltmc.server.registry.Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.wire.common.Identifier
                        .vanilla("jungle_leaves"),
                )?.stateId ?: error("unknown block")
        val mangroveRootsState =
            org.basaltmc.server.registry.Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.wire.common.Identifier
                        .vanilla("mangrove_roots"),
                )?.stateId ?: error("unknown block")
        val muddyMangroveRootsState =
            org.basaltmc.server.registry.Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.wire.common.Identifier
                        .vanilla("muddy_mangrove_roots"),
                )?.stateId ?: error("unknown block")
        val podzolState = org.basaltmc.server.registry.Blocks.PODZOL.stateId
        val paleMossBlockState =
            org.basaltmc.server.registry.Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.wire.common.Identifier
                        .vanilla("pale_moss_block"),
                )?.stateId ?: error("unknown block")
        val naturalDormantCreakingHeartState =
            (
                org.basaltmc.server.registry.Blocks.REGISTRY
                    .byIdentifier(
                        org.basaltmc.wire.common.Identifier
                            .vanilla("creaking_heart"),
                    )?.stateId ?: error("unknown block")
            ) + 1
    }
}
