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
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

/** parity: `HopperBlockEntity.tryMoveItems`, one item per turn and eight ticks of rest after it. */
class HoppersTest {
    private val world =
        LoadedWorld(
            WorldDeclaration("hoppers", WorldDimension.OVERWORLD, WorldType.VOID, seed = 0L, isDefault = true),
            WorldSettings.VANILLA,
            WorldMetadata(0L, 0, 64, 0),
            "build/test-data/hoppers",
            Logger("HoppersTest").apply { minLevel = LogLevel.ERROR },
            1,
        )

    private val stone = Identifier.vanilla("stone")

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
        properties: Map<String, String> = emptyMap(),
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

    @Test
    fun itPushesOneItemIntoWhatItFaces() {
        val chestData = place("chest", 3, 69, 3, mapOf("facing" to "north", "type" to "single"), size = 27)
        val hopperData =
            place(
                "hopper",
                3,
                70,
                3,
                mapOf("facing" to "down", "enabled" to "true"),
                contents = mapOf(0 to 3),
                size = Hoppers.SIZE,
            )

        assertEquals(true, Hoppers.tick(world, 3, 70, 3))

        assertEquals(2, BlockInventory.read(hopperData, Hoppers.SIZE)[0]?.count, "one item left the hopper")
        assertEquals(1, BlockInventory.read(chestData, 27)[0]?.count, "and landed in the chest")
    }

    /** parity: the eight tick cooldown, which is what makes a hopper move at two and a half items a second. */
    @Test
    fun itRestsForEightTicksAfterAMove() {
        place("chest", 6, 69, 3, mapOf("facing" to "north", "type" to "single"), size = 27)
        val hopperData =
            place(
                "hopper",
                6,
                70,
                3,
                mapOf("facing" to "down", "enabled" to "true"),
                contents = mapOf(0 to 64),
                size = Hoppers.SIZE,
            )

        Hoppers.tick(world, 6, 70, 3)
        repeat(8) { Hoppers.tick(world, 6, 70, 3) }

        assertEquals(63, BlockInventory.read(hopperData, Hoppers.SIZE)[0]?.count, "only the first move happened")
    }

    /** parity: `HopperBlock.ENABLED`: a powered hopper does nothing at all. */
    @Test
    fun aPoweredHopperStandsStill() {
        place("chest", 9, 69, 3, mapOf("facing" to "north", "type" to "single"), size = 27)
        val hopperData =
            place(
                "hopper",
                9,
                70,
                3,
                mapOf("facing" to "down", "enabled" to "false"),
                contents = mapOf(0 to 3),
                size = Hoppers.SIZE,
            )

        assertFalse(Hoppers.tick(world, 9, 70, 3))
        assertEquals(3, BlockInventory.read(hopperData, Hoppers.SIZE)[0]?.count)
    }

    /** parity: `suckInItems`, the container half: it pulls out of whatever sits on top of it. */
    @Test
    fun itPullsFromTheChestAboveIt() {
        val chestData =
            place(
                "chest",
                12,
                71,
                3,
                mapOf("facing" to "north", "type" to "single"),
                contents = mapOf(0 to 2),
                size = 27,
            )
        val hopperData =
            place("hopper", 12, 70, 3, mapOf("facing" to "down", "enabled" to "true"), size = Hoppers.SIZE)

        assertEquals(true, Hoppers.tick(world, 12, 70, 3))

        assertEquals(1, BlockInventory.read(chestData, 27)[0]?.count, "one left the chest")
        assertEquals(1, BlockInventory.read(hopperData, Hoppers.SIZE)[0]?.count, "and is in the hopper")
    }

    /**
     * parity: `AbstractFurnaceBlockEntity.getSlotsForFace`: from above a furnace only takes what it
     * is to cook, and its fuel slot is the one the sides open to.
     */
    @Test
    fun aHopperOverAFurnaceFillsTheCookingSlot() {
        val furnaceData =
            place("furnace", 15, 69, 3, mapOf("facing" to "north", "lit" to "false"), size = 3)
        place(
            "hopper",
            15,
            70,
            3,
            mapOf("facing" to "down", "enabled" to "true"),
            contents = mapOf(0 to 4),
            size = Hoppers.SIZE,
        )

        Hoppers.tick(world, 15, 70, 3)

        val slots = BlockInventory.read(furnaceData, 3)
        assertEquals(1, slots[0]?.count, "the ingredient slot took it")
        assertNull(slots[1], "and the fuel slot was left alone")
    }

    /** parity: `suckInItems`, which eats what is lying in the box above it. */
    @Test
    fun aHopperEatsWhatLiesOnIt() {
        val hopperData =
            place(
                "hopper",
                2,
                70,
                2,
                mapOf("facing" to "down", "enabled" to "true"),
                size = Hoppers.SIZE,
            )
        val dropped =
            world.state.spawnItem(
                WorldState.DroppedItem(
                    entityId = 41,
                    uuid = kotlin.uuid.Uuid.random(),
                    item = Identifier.vanilla("diamond"),
                    count = 3,
                    x = 2.5,
                    y = 70.9,
                    z = 2.5,
                ),
            )

        assertTrue(Hoppers.tick(world, 2, 70, 2))

        assertTrue(world.state.items().isEmpty(), "the item should be gone")
        assertEquals(3, BlockInventory.read(hopperData, Hoppers.SIZE).firstOrNull { it != null }?.count)
        assertEquals(0, dropped.count)
    }

    /** parity: the suck box starts at 11/16, so an item under the hopper is not eaten. */
    @Test
    fun anItemBelowItIsLeftAlone() {
        place("hopper", 3, 70, 2, mapOf("facing" to "down", "enabled" to "true"), size = Hoppers.SIZE)
        world.state.spawnItem(
            WorldState.DroppedItem(
                entityId = 42,
                uuid = kotlin.uuid.Uuid.random(),
                item = Identifier.vanilla("diamond"),
                count = 1,
                x = 3.5,
                y = 70.2,
                z = 3.5,
            ),
        )

        Hoppers.tick(world, 3, 70, 2)

        assertEquals(1, world.state.items().size, "it is below the mouth, so it stays")
    }
}
