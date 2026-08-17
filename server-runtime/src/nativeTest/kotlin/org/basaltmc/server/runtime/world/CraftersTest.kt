package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.config.WorldDeclaration
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.config.WorldSettings
import org.basaltmc.server.runtime.config.WorldType
import org.basaltmc.server.world.ChunkBlockEntity
import org.basaltmc.server.world.crafting.CraftingInput
import org.basaltmc.server.world.loot.ItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.LogLevel
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtIntArray
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull

/** parity: `CrafterBlock`, the block that is a container and a redstone component at once. */
class CraftersTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("crafters", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/crafters",
            Logger("CraftersTest").apply { minLevel = LogLevel.ERROR },
            1,
        )

    private val plank = Identifier.vanilla("oak_planks")
    private val stick = ItemStack(Identifier.vanilla("stick"), 4)

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

    /** A crafter at (2,70,2) facing north, with [filled] slots holding a plank each. */
    private fun crafterWith(filled: Int): ChunkBlockEntity {
        world.state.setBlockState(
            2,
            70,
            2,
            state("crafter", mapOf("orientation" to "north_up", "triggered" to "false", "crafting" to "false")),
        )
        val chunk = checkNotNull(world.state.chunkIfLoaded(0, 0))
        val entity = ChunkBlockEntity(2, 70, 2, Crafters.BLOCK, NbtCompound())
        val slots = arrayOfNulls<ItemStack>(Crafters.SIZE)
        for (slot in 0 until filled) slots[slot] = ItemStack(plank, 3)
        BlockInventory.write(entity.data, slots)
        chunk.setBlockEntity(entity)
        return entity
    }

    private fun propertiesAt(): Map<String, String> =
        BlockStateDefinitions.propertiesOf(world.state.getBlockState(2, 70, 2) ?: 0)

    /** parity: `neighborChanged`, which books four ticks on the rising edge and remembers it. */
    @Test
    fun aSignalBooksATurn() {
        crafterWith(filled = 2)

        assertNull(Crafters.onNeighbourChange(world, 2, 70, 2), "nothing powers it yet")

        world.state.setBlockState(2, 71, 2, state("redstone_block"))
        assertNotNull(Crafters.onNeighbourChange(world, 2, 70, 2))
        assertEquals("true", propertiesAt()["triggered"])
        val pending =
            world.state
                .chunkIfLoaded(0, 0)
                ?.scheduledBlockTicks()
                .orEmpty()
        assertEquals(1, pending.count { it.y == 70 && it.type.path == "crafter" && it.delay == 4 })

        world.state.setBlockState(2, 71, 2, Blocks.AIR.stateId)
        assertNotNull(Crafters.onNeighbourChange(world, 2, 70, 2))
        assertEquals("false", propertiesAt()["triggered"])
    }

    /**
     * parity: `dispenseFrom`: every slot with something in it loses exactly one, whatever the recipe
     * asked for, and the result goes out the front.
     */
    @Test
    fun itCraftsAndEverySlotPaysOne() {
        val entity = crafterWith(filled = 2)
        var asked: CraftingInput? = null

        val change =
            Crafters.craft(world, 2, 70, 2, { input ->
                asked = input
                stick
            }) { 1 }

        assertNotNull(change)
        assertEquals("true", propertiesAt()["crafting"])
        assertEquals(2, assertNotNull(asked).ingredientCount, "it offered what its slots hold")

        val left = BlockInventory.read(entity.data, Crafters.SIZE)
        assertEquals(2, left[0]?.count, "three minus one")
        assertEquals(2, left[1]?.count)
        assertNull(left[2], "and an empty slot pays nothing")

        // Nothing in front of it, so the result is on the floor one block north.
        val dropped = world.state.items()
        assertEquals(1, dropped.size)
        assertEquals(stick.item, dropped[0].item)
        assertEquals(4, dropped[0].count)
        assertEquals(1.5, dropped[0].z, "out of its front, which is north")
    }

    /** No recipe means nothing happens: no crafting flag, no items taken. */
    @Test
    fun anImpossibleGridCraftsNothing() {
        val entity = crafterWith(filled = 3)

        assertNull(Crafters.craft(world, 2, 70, 2, { null }) { 1 })

        assertEquals("false", propertiesAt()["crafting"])
        assertEquals(3, BlockInventory.read(entity.data, Crafters.SIZE)[0]?.count)
    }

    /** parity: `serverTick`, six ticks of animation and then the block goes still. */
    @Test
    fun theAnimationLastsSixTicks() {
        val entity = crafterWith(filled = 1)
        Crafters.craft(world, 2, 70, 2, { stick }) { 1 }

        repeat(5) { assertNull(Crafters.tick(world, 2, 70, 2, entity.data), "still crafting") }
        assertNotNull(Crafters.tick(world, 2, 70, 2, entity.data), "the sixth ends it")
        assertEquals("false", propertiesAt()["crafting"])
        assertNull(Crafters.tick(world, 2, 70, 2, entity.data), "and it stays still")
    }

    /** parity: `CrafterMenu`: nine of the block's own and then the player's, with no result slot. */
    @Test
    fun theWindowIsNineSlotsThenThePlayers() {
        val menu =
            org.basaltmc.server.runtime.player.Menu
                .crafter(containerId = 5)
        assertEquals(
            org.basaltmc.server.runtime.player.MenuSlot
                .Block(0),
            menu.slotAt(0),
        )
        assertEquals(
            org.basaltmc.server.runtime.player.MenuSlot
                .Block(8),
            menu.slotAt(8),
        )
        assertEquals(
            org.basaltmc.server.runtime.player.MenuSlot
                .Inventory(0),
            menu.slotAt(9),
        )
        assertEquals(
            org.basaltmc.server.runtime.player.MenuSlot
                .Inventory(35),
            menu.slotAt(44),
        )
        assertEquals(45, menu.slotCount)
    }

    /** parity: `getRedstoneSignal`, which counts slots: a slot switched off reads as taken. */
    @Test
    fun theComparatorCountsSlotsNotItems() {
        val entity = crafterWith(filled = 2)

        assertEquals(2, Crafters.comparatorSignal(entity.data))

        entity.data["disabled_slots"] = NbtIntArray(intArrayOf(7, 8))
        assertEquals(4, Crafters.comparatorSignal(entity.data), "two full and two switched off")
        assertEquals(false, Crafters.accepts(entity.data, 7), "and a hopper cannot fill one that is off")
        assertEquals(true, Crafters.accepts(entity.data, 3))
    }
}
