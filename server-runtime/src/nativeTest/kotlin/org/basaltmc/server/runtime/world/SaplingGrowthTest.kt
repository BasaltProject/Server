package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.BlockConnections
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class SaplingGrowthTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("saplings", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/saplings",
            Logger("SaplingTest").apply { minLevel = LogLevel.ERROR },
            1,
        )

    init {
        // The tree may reach into any of the nine, since it is planted at a chunk's own edge below.
        for (chunkX in -1..1) {
            for (chunkZ in -1..1) world.state.getOrGenerateChunk(chunkX, chunkZ)
        }
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun state(
        name: String,
        properties: Map<String, String> = emptyMap(),
    ): Int = checkNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla(name), properties))

    private fun pathAt(
        x: Int,
        y: Int,
        z: Int,
    ): String? = BlockStateDefinitions.blockOf(world.state.getBlockState(x, y, z) ?: 0)?.path

    /** parity: `SaplingBlock.advanceTree`, whose first turn only moves the stage on. */
    @Test
    fun theFirstTurnOnlyAdvancesTheStage() {
        world.state.setBlockState(4, 64, 4, Blocks.DIRT.stateId)
        world.state.setBlockState(4, 65, 4, state("oak_sapling", mapOf("stage" to "0")))

        val changed =
            SaplingGrowth.advance(
                world,
                4,
                65,
                4,
                state("oak_sapling", mapOf("stage" to "0")),
                LegacyRandom(1L),
            )

        assertEquals(1, changed.size)
        assertEquals("oak_sapling", pathAt(4, 65, 4))
        assertEquals(
            "1",
            BlockStateDefinitions.propertiesOf(checkNotNull(world.state.getBlockState(4, 65, 4)))["stage"],
        )
    }

    @Test
    fun aGrownSaplingBecomesATrunkAndLeaves() {
        world.state.setBlockState(6, 64, 6, Blocks.DIRT.stateId)
        val sapling = state("oak_sapling", mapOf("stage" to "1"))
        world.state.setBlockState(6, 65, 6, sapling)

        val changes = SaplingGrowth.advance(world, 6, 65, 6, sapling, LegacyRandom(5L))

        assertTrue(changes.isNotEmpty(), "it should have planted something")
        assertEquals("oak_log", pathAt(6, 65, 6), "the trunk starts where the sapling stood")
        assertTrue(
            changes.any { BlockConnections.isLeaves(it.stateId) },
            "and it should have a canopy",
        )
    }

    /**
     * The reason this grows on a scratch chunk: the decoration features mask their coordinates to
     * one chunk, so a tree planted against an edge used to wrap its canopy round to the far side.
     */
    @Test
    fun aTreeAtAChunkEdgeSpillsIntoTheNextChunkAndNotBackOnItself() {
        world.state.setBlockState(15, 64, 15, Blocks.DIRT.stateId)
        val sapling = state("oak_sapling", mapOf("stage" to "1"))
        world.state.setBlockState(15, 65, 15, sapling)

        val changes = SaplingGrowth.advance(world, 15, 65, 15, sapling, LegacyRandom(5L))

        assertTrue(changes.isNotEmpty(), "it should have planted something")
        assertTrue(changes.any { it.x > 15 || it.z > 15 }, "the canopy should reach the next chunk")
        // Everything it wrote is near the trunk, which is what wrapping would break.
        assertTrue(
            changes.all { kotlin.math.abs(it.x - 15) <= 8 && kotlin.math.abs(it.z - 15) <= 8 },
            "nothing should have appeared on the far side of the chunk",
        )
    }

    @Test
    fun everySaplingHasSomethingToGrowInto() {
        for (name in listOf(
            "oak_sapling",
            "birch_sapling",
            "spruce_sapling",
            "jungle_sapling",
            "acacia_sapling",
            "dark_oak_sapling",
            "pale_oak_sapling",
            "cherry_sapling",
            "mangrove_propagule",
        )) {
            assertTrue(SaplingGrowth.isSapling(state(name)), "$name should be known")
        }
        assertTrue(!SaplingGrowth.isSapling(Blocks.STONE.stateId))
    }
}
