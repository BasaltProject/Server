package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.server.world.loot.ItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals

/** parity: `ComparatorBlockEntity.getOutputSignal`, the level a comparator actually puts out. */
class ComparatorOutputTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("comparators", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/comparators",
            Logger("ComparatorOutputTest").apply { minLevel = LogLevel.ERROR },
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

    /**
     * A barrel one item short of full reads as one, and the comparator in front of it puts out
     * exactly that: not the flat fifteen a `powered` state on its own would give.
     */
    @Test
    fun theComparatorPutsOutWhatItRead() {
        world.state.setBlockState(2, 70, 2, state("barrel", mapOf("facing" to "up", "open" to "false")))
        val chunk = checkNotNull(world.state.chunkIfLoaded(0, 0))
        val entity =
            org.basaltmc.server.world
                .ChunkBlockEntity(
                    2,
                    70,
                    2,
                    Identifier.vanilla("barrel"),
                    org.basaltmc.wire.nbt
                        .NbtCompound(),
                )
        chunk.setBlockEntity(entity)
        val slots = arrayOfNulls<ItemStack>(27)
        slots[0] = ItemStack(Identifier.vanilla("emerald"), 64)
        slots[1] = ItemStack(Identifier.vanilla("emerald"), 64)
        BlockInventory.write(entity.data, slots)
        // Two full stacks out of twenty-seven slots: `1 + 2/27 * 14` floors to two.
        assertEquals(2, Containers.analogSignal(world, 2, 70, 2))

        // The comparator sits north of the barrel and reads it, facing the way it reads.
        world.state.setBlockState(
            2,
            70,
            1,
            state("comparator", mapOf("facing" to "south", "mode" to "compare", "powered" to "false")),
        )
        RedstoneComponents.tick(world, 2, 70, 1)

        assertEquals(2, Redstone.comparatorOutputAt(world, 2, 70, 1))
        val comparator = checkNotNull(world.state.getBlockState(2, 70, 1))
        assertEquals("true", BlockStateDefinitions.propertiesOf(comparator)["powered"])
        // What it feeds the block it points at is that same two, not fifteen.
        assertEquals(2, Redstone.signalFrom(world, 2, 70, 1, BlockFace.SOUTH))
    }

    /** parity: an empty container leaves the comparator dark, and its level at nothing. */
    @Test
    fun anEmptyContainerLeavesItDark() {
        world.state.setBlockState(6, 70, 2, state("barrel", mapOf("facing" to "up", "open" to "false")))
        world.state.setBlockState(
            6,
            70,
            1,
            state("comparator", mapOf("facing" to "south", "mode" to "compare", "powered" to "false")),
        )

        RedstoneComponents.tick(world, 6, 70, 1)

        assertEquals(0, Redstone.comparatorOutputAt(world, 6, 70, 1))
        assertEquals(0, Redstone.signalFrom(world, 6, 70, 1, BlockFace.SOUTH))
    }
}
