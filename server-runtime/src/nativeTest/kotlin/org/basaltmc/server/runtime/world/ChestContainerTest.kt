package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.server.runtime.player.Menu
import org.basaltmc.server.runtime.player.MenuSlot
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

/** parity: `ChestMenu.threeRows` and the contents a chest keeps in its own block entity. */
class ChestContainerTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("chests", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/chests",
            Logger("ChestContainerTest").apply { minLevel = LogLevel.ERROR },
            1,
        )

    init {
        world.state.getOrGenerateChunk(0, 0)
    }

    @AfterTest
    fun stopGenerating() {
        world.stopGenerating(drainTimeoutMillis = 100)
    }

    @Test
    fun theWindowIsTwentySevenSlotsThenThePlayers() {
        val menu = Menu.chest(containerId = 4)
        assertEquals(MenuSlot.Block(0), menu.slotAt(0))
        assertEquals(MenuSlot.Block(26), menu.slotAt(26), "three rows of nine")
        assertEquals(MenuSlot.Inventory(0), menu.slotAt(27), "then the storage")
        assertEquals(MenuSlot.Inventory(35), menu.slotAt(62), "and the hotbar last")
        assertNull(menu.slotAt(63))
    }

    /** The same NBT shape a furnace uses, so a chest saves and loads for free. */
    @Test
    fun itKeepsWhatIsPutInIt() {
        val data = NbtCompound()
        val slots = arrayOfNulls<ItemStack>(Menu.CHEST_SIZE)
        slots[0] = ItemStack(Identifier.vanilla("diamond"), 5)
        slots[26] = ItemStack(Identifier.vanilla("stone"), 64)
        BlockInventory.write(data, slots)

        val back = BlockInventory.read(data, Menu.CHEST_SIZE)

        assertEquals(Identifier.vanilla("diamond"), back[0]?.item)
        assertEquals(5, back[0]?.count)
        assertEquals(64, back[26]?.count)
        assertNull(back[13], "and the empty ones stay empty")
    }

    /** parity: a comparator behind a chest reads how full it is, the same as behind a furnace. */
    @Test
    fun aComparatorReadsAChest() {
        world.state.setBlockState(
            3,
            70,
            3,
            checkNotNull(
                BlockStateDefinitions.stateIdOf(
                    Identifier.vanilla("chest"),
                    mapOf("facing" to "north", "type" to "single", "waterlogged" to "false"),
                ),
            ),
        )
        val data = NbtCompound()
        val slots = arrayOfNulls<ItemStack>(Menu.CHEST_SIZE)
        slots[0] = ItemStack(Identifier.vanilla("stone"), 1)
        BlockInventory.write(data, slots)
        checkNotNull(world.state.chunkIfLoaded(0, 0))
            .setBlockEntity(ChunkBlockEntity(3, 70, 3, Identifier.vanilla("chest"), data))

        assertEquals(1, Containers.analogSignal(world, 3, 70, 3), "one item in twenty-seven still reads one")

        val full = arrayOfNulls<ItemStack>(Menu.CHEST_SIZE)
        for (slot in full.indices) full[slot] = ItemStack(Identifier.vanilla("stone"), 64)
        BlockInventory.write(data, full)
        assertEquals(15, Containers.analogSignal(world, 3, 70, 3), "and a full chest reads fifteen")
    }
}
