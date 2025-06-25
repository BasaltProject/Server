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
import kotlin.test.assertNotNull

/** What a hand or a foot puts into a circuit: levers, buttons and plates. */
class RedstoneInputTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("inputs", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/inputs",
            Logger("RedstoneInputTest").apply { minLevel = LogLevel.ERROR },
            1,
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
        properties: Map<String, String>,
    ): Int = checkNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla(name), properties))

    private fun litAt(
        x: Int,
        y: Int,
        z: Int,
    ) = BlockStateDefinitions.propertiesOf(world.state.getBlockState(x, y, z) ?: 0)["lit"]

    /** parity: `LeverBlock.useWithoutItem` into `updateNeighbours`: flipping it lights a lamp. */
    @Test
    fun aLeverLightsALampNextToIt() {
        world.state.setBlockState(2, 70, 2, state("redstone_lamp", mapOf("lit" to "false")))
        world.state.setBlockState(
            3,
            70,
            2,
            state("lever", mapOf("face" to "floor", "facing" to "north", "powered" to "false")),
        )
        world.state.setBlockState(3, 69, 2, state("stone", emptyMap()))

        val toggled = assertNotNull(BlockInteractions.toggle(world, 3, 70, 2))
        for (block in toggled.changes) {
            NeighbourUpdates.afterChange(world, block[0], block[1], block[2]) { _, _, _, _ -> }
        }

        assertEquals("true", litAt(2, 70, 2), "the lamp beside the lever")
    }

    /** parity: `BasePressurePlateBlock.tick`, the turn a plate books so it can come back up. */
    @Test
    fun aPlateComesBackUpWhenNobodyIsOnIt() {
        world.state.setBlockState(6, 69, 2, state("stone", emptyMap()))
        world.state.setBlockState(6, 70, 2, state("oak_pressure_plate", mapOf("powered" to "false")))

        val pressed = assertNotNull(PressurePlates.update(world, 6, 70, 2, standing = 1))
        assertEquals("true", BlockStateDefinitions.propertiesOf(pressed.stateId)["powered"])

        // Whoever was on it walks away, and the plate's own scheduled turn comes round.
        val released = assertNotNull(PressurePlates.update(world, 6, 70, 2, standing = 0))
        assertEquals("false", BlockStateDefinitions.propertiesOf(released.stateId)["powered"])
    }

    /** The plate has to book that turn, or nothing ever asks it again. */
    @Test
    fun aPressedPlateBooksItsOwnRecheck() {
        world.state.setBlockState(9, 69, 2, state("stone", emptyMap()))
        world.state.setBlockState(9, 70, 2, state("oak_pressure_plate", mapOf("powered" to "false")))

        PressurePlates.update(world, 9, 70, 2, standing = 1)

        assertEquals(
            true,
            world.state.hasScheduledBlockTick(9, 70, 2, Identifier.vanilla("oak_pressure_plate")),
        )
    }
}
