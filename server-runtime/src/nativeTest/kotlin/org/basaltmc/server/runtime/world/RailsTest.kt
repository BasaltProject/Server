package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
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

/** parity: `RailState.place`, which is how a track laid one rail at a time follows the person laying it. */
class RailsTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("rails", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/rails",
            Logger("RailsTest").apply { minLevel = LogLevel.ERROR },
            1,
        )

    init {
        world.state.getOrGenerateChunk(0, 0)
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    private fun rail(
        x: Int,
        y: Int,
        z: Int,
        block: String = "rail",
        shape: String = "north_south",
    ) {
        val properties =
            if (block == "rail") {
                mapOf("shape" to shape, "waterlogged" to "false")
            } else {
                mapOf("shape" to shape, "powered" to "false", "waterlogged" to "false")
            }
        world.state.setBlockState(
            x,
            y,
            z,
            checkNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla(block), properties)),
        )
        Rails.place(world, x, y, z)
    }

    private fun shapeAt(
        x: Int,
        y: Int,
        z: Int,
    ) = BlockStateDefinitions.propertiesOf(world.state.getBlockState(x, y, z) ?: 0)["shape"]

    /** Two rails side by side along x end up as one east-west line, whatever they were placed as. */
    @Test
    fun railsInARowLineUp() {
        rail(2, 70, 2)
        rail(3, 70, 2)

        assertEquals("east_west", shapeAt(2, 70, 2))
        assertEquals("east_west", shapeAt(3, 70, 2))
    }

    /** parity: a plain rail turns a corner where a powered one may not. */
    @Test
    fun aPlainRailTurnsTheCorner() {
        rail(6, 70, 2)
        rail(7, 70, 2)
        rail(7, 70, 3)

        assertEquals("south_west", shapeAt(7, 70, 2), "the corner joining west and south")
        assertEquals("north_south", shapeAt(7, 70, 3))
    }

    /** parity: `isStraight`, which is true for the three rails that carry a signal. */
    @Test
    fun aPoweredRailNeverTurns() {
        rail(10, 70, 2)
        rail(11, 70, 2, block = "powered_rail")
        rail(11, 70, 3)

        val shape = shapeAt(11, 70, 2)
        assertEquals(true, shape == "east_west" || shape == "north_south", "it stayed straight: $shape")
    }

    /** parity: a rail one block higher makes the one below it climb. */
    @Test
    fun aRailClimbsTowardsOneAbove() {
        rail(2, 70, 6)
        rail(3, 71, 6)

        assertEquals("ascending_east", shapeAt(2, 70, 6))
    }
}
