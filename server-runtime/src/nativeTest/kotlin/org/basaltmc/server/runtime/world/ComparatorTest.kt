package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.server.world.ChunkBlockEntity
import org.basaltmc.server.world.loot.ItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.nbt.NbtCompound
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull

/**
 * parity: `ComparatorBlock` and `AbstractContainerMenu.getRedstoneSignalFromContainer`, which is
 * where the two systems meet: a comparator is redstone reading how full a container is.
 */
class ComparatorTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("comparator", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/comparator",
            Logger("ComparatorTest").apply { minLevel = LogLevel.ERROR },
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
        properties: Map<String, String> = emptyMap(),
    ): Int = checkNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla(name), properties))

    /** Puts a furnace at these coordinates holding [counts] in its three slots. */
    private fun furnace(
        x: Int,
        y: Int,
        z: Int,
        vararg counts: Int,
    ) {
        world.state.setBlockState(x, y, z, state("furnace", mapOf("facing" to "north", "lit" to "false")))
        val furnaceState = Furnaces.State()
        for (slot in counts.indices) {
            if (counts[slot] > 0) furnaceState.items[slot] = ItemStack(Identifier.vanilla("coal"), counts[slot])
        }
        val data = NbtCompound()
        Furnaces.write(furnaceState, data)
        val chunk = checkNotNull(world.state.chunkIfLoaded(0, 0))
        chunk.setBlockEntity(ChunkBlockEntity(x and 15, y, z and 15, Identifier.vanilla("furnace"), data))
    }

    /** parity: `Mth.lerpDiscrete`: anything at all reads one, and a full container reads fifteen. */
    @Test
    fun aComparatorReadsHowFullTheFurnaceIs() {
        furnace(2, 70, 2, 0, 0, 0)
        assertEquals(0, Containers.analogSignal(world, 2, 70, 2), "an empty furnace reads nothing")

        furnace(3, 70, 2, 1, 0, 0)
        assertEquals(1, Containers.analogSignal(world, 3, 70, 2), "one item already reads one")

        furnace(4, 70, 2, 64, 64, 64)
        assertEquals(15, Containers.analogSignal(world, 4, 70, 2), "and a full one reads fifteen")

        furnace(5, 70, 2, 64, 0, 0)
        assertEquals(5, Containers.analogSignal(world, 5, 70, 2), "a third full is a third of the way up")
    }

    /** A block that keeps no contents answers nothing at all, rather than an empty box. */
    @Test
    fun aPlainBlockIsNotAContainer() {
        world.state.setBlockState(7, 70, 2, state("stone"))
        assertNull(Containers.analogSignal(world, 7, 70, 2))
    }

    /** parity: `ComparatorBlock.getInputSignal`, which reads the container it faces. */
    @Test
    fun aComparatorTurnsOnBehindAFullFurnace() {
        furnace(9, 70, 2, 64, 64, 64)
        // Facing west, so its input is the furnace at x=9.
        world.state.setBlockState(
            10,
            70,
            2,
            state("comparator", mapOf("facing" to "west", "mode" to "compare", "powered" to "false")),
        )

        val turned = checkNotNull(RedstoneComponents.tick(world, 10, 70, 2))

        assertEquals("true", BlockStateDefinitions.propertiesOf(turned.stateId)["powered"])
    }

    /** And stays off behind an empty one. */
    @Test
    fun anEmptyFurnaceLeavesItOff() {
        furnace(12, 70, 2, 0, 0, 0)
        world.state.setBlockState(
            13,
            70,
            2,
            state("comparator", mapOf("facing" to "west", "mode" to "compare", "powered" to "false")),
        )

        assertNull(RedstoneComponents.tick(world, 13, 70, 2), "nothing to report")
    }

    /**
     * parity: the `getAnalogOutputSignal`s that read the block's own state rather than any contents:
     * a cake counts its slices, a cauldron and a composter their level, an anchor its charges.
     */
    @Test
    fun someBlocksAreReadStraightOffTheirState() {
        fun readingOf(
            name: String,
            properties: Map<String, String>,
        ): Int? {
            world.state.setBlockState(6, 78, 6, state(name, properties))
            return Containers.analogSignal(world, 6, 78, 6)
        }

        assertEquals(14, readingOf("cake", mapOf("bites" to "0")), "a whole cake is fourteen")
        assertEquals(8, readingOf("cake", mapOf("bites" to "3")))
        assertEquals(14, readingOf("candle_cake", mapOf("lit" to "false")), "a cake with a candle is whole")
        assertEquals(5, readingOf("composter", mapOf("level" to "5")))
        assertEquals(2, readingOf("water_cauldron", mapOf("level" to "2")))
        assertEquals(3, readingOf("lava_cauldron", emptyMap()), "lava has no levels, so it is always three")
        assertEquals(0, readingOf("respawn_anchor", mapOf("charges" to "0")))
        assertEquals(7, readingOf("respawn_anchor", mapOf("charges" to "2")), "two of four, floored")
        assertEquals(15, readingOf("respawn_anchor", mapOf("charges" to "4")))
        assertEquals(15, readingOf("end_portal_frame", mapOf("eye" to "true", "facing" to "north")))
        assertEquals(0, readingOf("end_portal_frame", mapOf("eye" to "false", "facing" to "north")))
        assertEquals(4, readingOf("beehive", mapOf("honey_level" to "4", "facing" to "north")))
        assertEquals(15, readingOf("copper_bulb", mapOf("lit" to "true", "powered" to "false")))
        assertEquals(0, readingOf("copper_bulb", mapOf("lit" to "false", "powered" to "false")))
        assertEquals(
            1,
            readingOf(
                "copper_golem_statue",
                mapOf("copper_golem_pose" to "standing", "facing" to "north", "waterlogged" to "false"),
            ),
        )
        assertEquals(
            4,
            readingOf(
                "copper_golem_statue",
                mapOf(
                    "copper_golem_pose" to "star",
                    "facing" to "north",
                    "waterlogged" to "false",
                ),
            ),
            "one per pose, in the order the enum declares them",
        )
    }
}
