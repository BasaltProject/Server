package org.basaltmc.world.terrain.decoration.tree

import org.basaltmc.nbt.NbtCompound
import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.world.Chunk
import org.basaltmc.world.terrain.decoration.TreeDecoration
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class TreeFeatureTest {
    private fun chunkWithFlatGround(groundY: Int): Chunk {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                chunk.setBlockStateId(x, groundY, z, org.basaltmc.registry.Blocks.GRASS_BLOCK.stateId)
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
    fun oakAppliesTheDefaultBelowTrunkDirtProvider() {
        val grass = chunkWithFlatGround(0)
        assertTrue(TreeFeature.place(grass, WorldgenRandom(XoroshiroRandom(42L)), 8, 1, 8, TreeConfigs.OAK))
        assertTrue(grass.getBlockStateId(8, 0, 8) == org.basaltmc.registry.Blocks.DIRT.stateId)

        val podzol = chunkWithFlatGround(0)
        podzol.setBlockStateId(8, 0, 8, org.basaltmc.registry.Blocks.PODZOL.stateId)
        assertTrue(TreeFeature.place(podzol, WorldgenRandom(XoroshiroRandom(42L)), 8, 1, 8, TreeConfigs.OAK))
        assertTrue(podzol.getBlockStateId(8, 0, 8) == org.basaltmc.registry.Blocks.PODZOL.stateId)
    }

    @Test
    fun darkOakAppliesTheBelowTrunkProviderAcrossItsTwoByTwoFootprint() {
        val chunk = chunkWithFlatGround(0)
        assertTrue(TreeFeature.place(chunk, WorldgenRandom(XoroshiroRandom(31L)), 7, 1, 7, TreeConfigs.DARK_OAK))
        for (x in 7..8) {
            for (z in 7..8) assertTrue(chunk.getBlockStateId(x, 0, z) == org.basaltmc.registry.Blocks.DIRT.stateId)
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
                        if (org.basaltmc.registry.Blocks.isVineState(
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
                        if (org.basaltmc.registry.Blocks
                                .isCocoaState(stateId)
                        ) {
                            cocoaBlocks++
                        }
                        if (org.basaltmc.registry.Blocks
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
                        if (org.basaltmc.registry.Blocks
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
    fun creakingPaleOakCanReplaceASurroundedLogWithANaturalDormantHeart() {
        var hearts = 0
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
            if (hearts > 0) break
        }
        assertTrue(hearts > 0, "expected at least one generated dark-oak shape to contain a valid heart position")
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
    fun cherryPlacesLogsAndLeaves() {
        val chunk = chunkWithFlatGround(0)
        val random = WorldgenRandom(XoroshiroRandom(99L))
        val placed = TreeFeature.place(chunk, random, 8, 1, 8, TreeConfigs.CHERRY)
        assertTrue(placed, "expected the cherry tree to place successfully")

        var logs = 0
        var leaves = 0
        for (x in 0 until 16) {
            for (y in 1..24) {
                for (z in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    if (id == cherryLogState) logs++
                    if (isLeavesOf(id, cherryLeavesState)) leaves++
                }
            }
        }
        // baseHeight 7 plus at least one side branch: well above a plain straight trunk's 7 logs.
        assertTrue(logs >= 8, "expected at least 8 cherry logs (trunk plus a branch), got $logs")
        assertTrue(leaves > 0, "expected at least one cherry leaves block")
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
    fun mangroveCanHangAgedPropagulesFromLeaves() {
        var propaguleState: Int? = null
        for (seed in 0L..255L) {
            val chunk = chunkWithFlatGround(0)
            assertTrue(TreeFeature.place(chunk, WorldgenRandom(XoroshiroRandom(seed)), 8, 1, 8, TreeConfigs.MANGROVE))
            outer@ for (x in 0 until 16) {
                for (z in 0 until 16) {
                    for (y in 1..24) {
                        val stateId = chunk.getBlockStateId(x, y, z)
                        if (org.basaltmc.registry.Blocks
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
            org.basaltmc.registry.Blocks
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

        val states = mutableListOf<org.basaltmc.registry.BlockState>()
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in 1..4) {
                    val stateId = chunk.getBlockStateId(x, y, z)
                    if (org.basaltmc.registry.Blocks
                            .isLeafLitterState(stateId)
                    ) {
                        states += org.basaltmc.registry.Blocks
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
        var beeNest: org.basaltmc.registry.BlockState? = null
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
                        if (org.basaltmc.registry.Blocks
                                .isBeeNestState(stateId)
                        ) {
                            beeNest =
                                org.basaltmc.registry.Blocks
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
            org.basaltmc.registry.Identifier
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
                            org.basaltmc.registry.Blocks
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
        val oakLogState =
            org.basaltmc.registry.Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.registry.Identifier
                        .vanilla("oak_log"),
                )?.stateId ?: error("unknown block")
        val oakLeavesState =
            org.basaltmc.registry.Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.registry.Identifier
                        .vanilla("oak_leaves"),
                )?.stateId ?: error("unknown block")
        val spruceLogState =
            org.basaltmc.registry.Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.registry.Identifier
                        .vanilla("spruce_log"),
                )?.stateId ?: error("unknown block")
        val spruceLeavesState =
            org.basaltmc.registry.Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.registry.Identifier
                        .vanilla("spruce_leaves"),
                )?.stateId ?: error("unknown block")
        val darkOakLogState =
            org.basaltmc.registry.Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.registry.Identifier
                        .vanilla("dark_oak_log"),
                )?.stateId ?: error("unknown block")
        val darkOakLeavesState =
            org.basaltmc.registry.Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.registry.Identifier
                        .vanilla("dark_oak_leaves"),
                )?.stateId ?: error("unknown block")
        val mangroveLogState =
            org.basaltmc.registry.Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.registry.Identifier
                        .vanilla("mangrove_log"),
                )?.stateId ?: error("unknown block")
        val mangroveLeavesState =
            org.basaltmc.registry.Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.registry.Identifier
                        .vanilla("mangrove_leaves"),
                )?.stateId ?: error("unknown block")
        val cherryLogState =
            org.basaltmc.registry.Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.registry.Identifier
                        .vanilla("cherry_log"),
                )?.stateId ?: error("unknown block")
        val cherryLeavesState =
            org.basaltmc.registry.Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.registry.Identifier
                        .vanilla("cherry_leaves"),
                )?.stateId ?: error("unknown block")
        val jungleLogState =
            org.basaltmc.registry.Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.registry.Identifier
                        .vanilla("jungle_log"),
                )?.stateId ?: error("unknown block")
        val jungleLeavesState =
            org.basaltmc.registry.Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.registry.Identifier
                        .vanilla("jungle_leaves"),
                )?.stateId ?: error("unknown block")
        val mangroveRootsState =
            org.basaltmc.registry.Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.registry.Identifier
                        .vanilla("mangrove_roots"),
                )?.stateId ?: error("unknown block")
        val podzolState = org.basaltmc.registry.Blocks.PODZOL.stateId
        val paleMossBlockState =
            org.basaltmc.registry.Blocks.REGISTRY
                .byIdentifier(
                    org.basaltmc.registry.Identifier
                        .vanilla("pale_moss_block"),
                )?.stateId ?: error("unknown block")
        val naturalDormantCreakingHeartState =
            (
                org.basaltmc.registry.Blocks.REGISTRY
                    .byIdentifier(
                        org.basaltmc.registry.Identifier
                            .vanilla("creaking_heart"),
                    )?.stateId ?: error("unknown block")
            ) + 1
    }
}
