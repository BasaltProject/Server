package org.basaltmc.world.terrain.decoration.tree

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.world.Chunk
import kotlin.test.Test
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
                    if (id == oakLeavesState) leaves++
                }
            }
        }
        assertTrue(logs > 0, "expected at least one oak log")
        assertTrue(leaves > 0, "expected at least one oak leaves block")
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
                    if (id == spruceLeavesState) leaves++
                }
            }
        }
        assertTrue(logs > 0, "expected at least one spruce log")
        assertTrue(leaves > 0, "expected at least one spruce leaves block")
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
                    if (id == darkOakLeavesState) leaves++
                }
            }
        }
        // A 2x2 trunk of at least baseHeight 6 must place at least 4*6 = 24 logs.
        assertTrue(logs >= 24, "expected at least 24 dark oak logs (2x2 trunk), got $logs")
        assertTrue(leaves > 0, "expected at least one dark oak leaves block")
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
                    if (id == spruceLeavesState) leaves++
                }
            }
        }
        // baseHeight 13 minimum, 2x2 trunk for all but the topmost layer: at least 13*3+1 = 40 logs.
        assertTrue(logs >= 40, "expected at least 40 spruce logs (2x2 giant trunk), got $logs")
        assertTrue(leaves > 0, "expected at least one spruce leaves block")
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
                    if (id == cherryLeavesState) leaves++
                }
            }
        }
        // baseHeight 7 plus at least one side branch: well above a plain straight trunk's 7 logs.
        assertTrue(logs >= 8, "expected at least 8 cherry logs (trunk plus a branch), got $logs")
        assertTrue(leaves > 0, "expected at least one cherry leaves block")
    }

    @Test
    fun mangrovePlacesLogsAndScatteredLeaves() {        val chunk = chunkWithFlatGround(0)
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
                    if (id == mangroveLeavesState) leaves++
                    if (id == mangroveRootsState) roots++
                }
            }
        }
        assertTrue(logs > 0, "expected at least one mangrove log")
        assertTrue(leaves > 0, "expected at least one mangrove leaves block")
        assertTrue(roots > 0, "expected at least one mangrove roots block (the trunk starts elevated)")
    }

    private companion object {
        val oakLogState =
            org.basaltmc.registry.Blocks.REGISTRY
                .byIdentifier(org.basaltmc.registry.Identifier.vanilla("oak_log"))
                ?.stateId ?: error("unknown block")
        val oakLeavesState =
            org.basaltmc.registry.Blocks.REGISTRY
                .byIdentifier(org.basaltmc.registry.Identifier.vanilla("oak_leaves"))
                ?.stateId ?: error("unknown block")
        val spruceLogState =
            org.basaltmc.registry.Blocks.REGISTRY
                .byIdentifier(org.basaltmc.registry.Identifier.vanilla("spruce_log"))
                ?.stateId ?: error("unknown block")
        val spruceLeavesState =
            org.basaltmc.registry.Blocks.REGISTRY
                .byIdentifier(org.basaltmc.registry.Identifier.vanilla("spruce_leaves"))
                ?.stateId ?: error("unknown block")
        val darkOakLogState =
            org.basaltmc.registry.Blocks.REGISTRY
                .byIdentifier(org.basaltmc.registry.Identifier.vanilla("dark_oak_log"))
                ?.stateId ?: error("unknown block")
        val darkOakLeavesState =
            org.basaltmc.registry.Blocks.REGISTRY
                .byIdentifier(org.basaltmc.registry.Identifier.vanilla("dark_oak_leaves"))
                ?.stateId ?: error("unknown block")
        val mangroveLogState =
            org.basaltmc.registry.Blocks.REGISTRY
                .byIdentifier(org.basaltmc.registry.Identifier.vanilla("mangrove_log"))
                ?.stateId ?: error("unknown block")
        val mangroveLeavesState =
            org.basaltmc.registry.Blocks.REGISTRY
                .byIdentifier(org.basaltmc.registry.Identifier.vanilla("mangrove_leaves"))
                ?.stateId ?: error("unknown block")
        val cherryLogState =
            org.basaltmc.registry.Blocks.REGISTRY
                .byIdentifier(org.basaltmc.registry.Identifier.vanilla("cherry_log"))
                ?.stateId ?: error("unknown block")
        val cherryLeavesState =
            org.basaltmc.registry.Blocks.REGISTRY
                .byIdentifier(org.basaltmc.registry.Identifier.vanilla("cherry_leaves"))
                ?.stateId ?: error("unknown block")
        val mangroveRootsState =
            org.basaltmc.registry.Blocks.REGISTRY
                .byIdentifier(org.basaltmc.registry.Identifier.vanilla("mangrove_roots"))
                ?.stateId ?: error("unknown block")
    }
}
