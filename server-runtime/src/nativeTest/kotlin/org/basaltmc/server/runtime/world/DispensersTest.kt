package org.basaltmc.server.runtime.world

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.server.registry.Blocks
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
import kotlin.test.assertTrue

/** parity: `DispenserBlock` and `DropperBlock`, which differ only in what they do with the item. */
class DispensersTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("dispensers", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/dispensers",
            Logger("DispensersTest").apply { minLevel = LogLevel.ERROR },
            1,
        )

    private val stone = Identifier.vanilla("stone")
    private var nextId = 100

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

    private fun place(
        name: String,
        x: Int,
        y: Int,
        z: Int,
        properties: Map<String, String>,
        contents: Map<Int, Int> = emptyMap(),
        size: Int,
    ): NbtCompound {
        world.state.setBlockState(x, y, z, state(name, properties))
        val data = NbtCompound()
        val slots = arrayOfNulls<ItemStack>(size)
        for ((slot, count) in contents) slots[slot] = ItemStack(stone, count)
        BlockInventory.write(data, slots)
        checkNotNull(world.state.chunkIfLoaded(0, 0))
            .setBlockEntity(ChunkBlockEntity(x and 15, y, z and 15, Identifier.vanilla(name), data))
        return data
    }

    /** parity: `neighborChanged`, which listens at its own block and the one above, on the rising edge. */
    @Test
    fun itBooksATurnWhenThePowerArrives() {
        place("dispenser", 2, 70, 2, mapOf("facing" to "north", "triggered" to "false"), size = 9)
        world.state.setBlockState(2, 71, 2, state("redstone_block"))

        val turned = checkNotNull(Dispensers.onNeighbourChange(world, 2, 70, 2))

        assertEquals("true", BlockStateDefinitions.propertiesOf(turned.stateId)["triggered"])
        val pending =
            world.state
                .chunkIfLoaded(0, 0)
                ?.scheduledBlockTicks()
                .orEmpty()
        assertTrue(pending.any { it.localX == 2 && it.y == 70 && it.delay == 4 }, "four ticks: $pending")

        assertNull(Dispensers.onNeighbourChange(world, 2, 70, 2), "and it does not book a second one")
    }

    /** parity: `DropperBlock.dispenseFrom`, which puts it in the container it faces. */
    @Test
    fun aDropperFacingAChestFillsIt() {
        val chest = place("chest", 5, 70, 2, mapOf("facing" to "north", "type" to "single"), size = 27)
        val dropper =
            place("dropper", 5, 71, 2, mapOf("facing" to "down", "triggered" to "false"), mapOf(0 to 2), size = 9)

        assertTrue(Dispensers.fire(world, 5, 71, 2, LegacyRandom(1L)) { nextId++ })

        assertEquals(1, BlockInventory.read(chest, 27)[0]?.count, "one item went in")
        assertEquals(1, BlockInventory.read(dropper, 9)[0]?.count, "and one is left")
    }

    /** With nothing in front of it, the item is thrown into the world. */
    @Test
    fun withNothingInFrontItThrowsTheItem() {
        val dropper =
            place("dropper", 8, 70, 2, mapOf("facing" to "north", "triggered" to "false"), mapOf(3 to 1), size = 9)
        val before = world.state.items().size

        assertTrue(Dispensers.fire(world, 8, 70, 2, LegacyRandom(1L)) { nextId++ })

        assertEquals(before + 1, world.state.items().size, "an item is now lying in the world")
        assertNull(BlockInventory.read(dropper, 9)[3], "and the slot is empty")
    }

    /**
     * parity: the filled-bucket behaviour, which pours instead of throwing the bucket, and leaves an
     * empty bucket in the slot.
     */
    @Test
    fun aDispenserPoursAWaterBucket() {
        world.state.setBlockState(14, 70, 2, Blocks.STONE.stateId)
        val data =
            place(
                "dispenser",
                14,
                71,
                2,
                mapOf("facing" to "up", "triggered" to "false"),
                size = 9,
            )
        val slots = arrayOfNulls<org.basaltmc.server.world.loot.ItemStack>(9)
        slots[0] =
            org.basaltmc.server.world.loot
                .ItemStack(Identifier.vanilla("water_bucket"), 1)
        BlockInventory.write(data, slots)

        assertTrue(Dispensers.fire(world, 14, 71, 2, LegacyRandom(1L)) { nextId++ })

        assertEquals(
            "water",
            BlockStateDefinitions.blockOf(world.state.getBlockState(14, 72, 2) ?: 0)?.path,
            "the water went where it was pointed",
        )
        assertEquals(
            Identifier.vanilla("bucket"),
            BlockInventory.read(data, 9)[0]?.item,
            "and the empty bucket stayed behind",
        )
    }

    /** parity: `FlintAndSteelDispenseItemBehavior`, which lights the block in front of it. */
    @Test
    fun aDispenserLightsAFire() {
        // Pointed sideways at a space that has a floor, which is what fire needs to stand on.
        world.state.setBlockState(13, 74, 1, Blocks.STONE.stateId)
        val data =
            place("dispenser", 13, 75, 2, mapOf("facing" to "north", "triggered" to "false"), size = 9)
        val slots = arrayOfNulls<org.basaltmc.server.world.loot.ItemStack>(9)
        slots[0] =
            org.basaltmc.server.world.loot
                .ItemStack(Identifier.vanilla("flint_and_steel"), 1)
        BlockInventory.write(data, slots)

        assertTrue(Dispensers.fire(world, 13, 75, 2, LegacyRandom(1L)) { nextId++ })

        assertEquals("fire", BlockStateDefinitions.blockOf(world.state.getBlockState(13, 75, 1) ?: 0)?.path)
    }

    /** An empty dispenser does nothing at all. */
    @Test
    fun anEmptyOneDoesNothing() {
        place("dispenser", 11, 70, 2, mapOf("facing" to "north", "triggered" to "false"), size = 9)

        assertEquals(false, Dispensers.fire(world, 11, 70, 2, LegacyRandom(1L)) { nextId++ })
    }
}
