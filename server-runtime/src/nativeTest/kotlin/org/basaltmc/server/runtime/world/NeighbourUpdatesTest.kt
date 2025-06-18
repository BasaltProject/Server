package org.basaltmc.server.runtime.world

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
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class NeighbourUpdatesTest {
    private val world =
        LoadedWorld(
            declaration =
                WorldDeclaration(
                    "neighbours",
                    WorldDimension.OVERWORLD,
                    WorldType.VOID,
                    seed = 0L,
                    isDefault = true,
                ),
            settings = WorldSettings.VANILLA,
            metadata = WorldMetadata(0L, 0, 64, 0),
            worldsRoot = "build/test-data/neighbours",
            logger = Logger("NeighbourUpdatesTest").apply { minLevel = LogLevel.ERROR },
            generationWorkers = 1,
        )

    init {
        world.state.getOrGenerateChunk(0, 0)
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun state(
        name: String,
        properties: Map<String, String> = emptyMap(),
    ): Int = checkNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla(name), properties))

    private fun at(
        x: Int,
        y: Int,
        z: Int,
    ) = world.state.getBlockState(x, y, z)

    /** Everything the cascade broke, in the order it went. */
    private fun dig(
        x: Int,
        y: Int,
        z: Int,
    ): List<Int> {
        world.state.setBlockState(x, y, z, Blocks.AIR.stateId)
        val broken = mutableListOf<Int>()
        NeighbourUpdates.afterChange(world, x, y, z) { stateId, _, _, _ -> broken += stateId }
        return broken
    }

    @Test
    fun grassFallsWhenTheBlockUnderItGoes() {
        val grass = state("short_grass")
        world.state.setBlockState(1, 64, 1, Blocks.DIRT.stateId)
        world.state.setBlockState(1, 65, 1, grass)

        assertEquals(listOf(grass), dig(1, 64, 1))
        assertEquals(Blocks.AIR.stateId, at(1, 65, 1))
    }

    @Test
    fun aTorchOnTheWallFallsWithTheWall() {
        val torch = state("wall_torch", mapOf("facing" to "north"))
        world.state.setBlockState(2, 64, 2, Blocks.STONE.stateId)
        world.state.setBlockState(2, 64, 1, torch)

        assertEquals(listOf(torch), dig(2, 64, 2))
    }

    /** One update, the whole stack: each block that goes tells the ones around it in turn. */
    @Test
    fun aColumnOfFlowersComesDownInOneGo() {
        val flower = state("dandelion")
        world.state.setBlockState(3, 64, 1, Blocks.DIRT.stateId)
        world.state.setBlockState(3, 65, 1, Blocks.DIRT.stateId)
        world.state.setBlockState(3, 66, 1, flower)
        // Not a real vanilla stack, but the shape of the cascade is what is under test: the dirt
        // goes, the flower on it goes, and nothing is left floating.
        assertEquals(listOf(flower), dig(3, 65, 1))
        assertEquals(Blocks.AIR.stateId, at(3, 66, 1))
    }

    /**
     * parity: `DoorBlock.updateShape` turns the top half into air rather than breaking it, so a
     * door drops one door and not two. Only the half that was mined counts as broken.
     */
    @Test
    fun bothHalvesOfADoorGoTogether() {
        val lower = state("oak_door", mapOf("half" to "lower", "facing" to "north"))
        val upper = state("oak_door", mapOf("half" to "upper", "facing" to "north"))
        world.state.setBlockState(4, 64, 1, Blocks.STONE.stateId)
        world.state.setBlockState(4, 65, 1, lower)
        world.state.setBlockState(4, 66, 1, upper)

        val broken = dig(4, 64, 1)

        assertEquals(listOf(lower), broken, "only the half that was mined drops anything")
        assertEquals(Blocks.AIR.stateId, at(4, 66, 1))
    }

    @Test
    fun anOrdinaryBlockIsLeftAlone() {
        world.state.setBlockState(5, 64, 1, Blocks.STONE.stateId)
        world.state.setBlockState(5, 65, 1, Blocks.STONE.stateId)

        assertTrue(dig(5, 64, 1).isEmpty())
        assertEquals(Blocks.STONE.stateId, at(5, 65, 1))
    }

    @Test
    fun aTorchCannotBePlacedOnNothing() {
        val torch = state("torch")
        assertFalse(NeighbourUpdates.isSupported(world, 6, 70, 1, torch))
        world.state.setBlockState(6, 69, 1, Blocks.STONE.stateId)
        assertTrue(NeighbourUpdates.isSupported(world, 6, 70, 1, torch))
    }

    /** Sugar cane keeps standing next to water and goes when the water is gone. */
    @Test
    fun sugarCaneAsksAboutTheWaterBesideTheSandUnderIt() {
        val cane = state("sugar_cane")
        world.state.setBlockState(7, 64, 1, state("sand"))
        world.state.setBlockState(7, 65, 1, cane)
        world.state.setBlockState(8, 64, 1, Blocks.WATER.stateId)

        assertTrue(NeighbourUpdates.isSupported(world, 7, 65, 1, cane))

        world.state.setBlockState(8, 64, 1, Blocks.AIR.stateId)
        assertFalse(NeighbourUpdates.isSupported(world, 7, 65, 1, cane))
    }
}
