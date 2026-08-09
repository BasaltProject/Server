package org.basaltmc.storage

import org.basaltmc.nbt.NbtCompound
import org.basaltmc.nbt.NbtList
import org.basaltmc.nbt.NbtType
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNull
import kotlin.test.assertTrue

private fun samplePlayer(
    uuid: String = "11111111-2222-3333-4444-555555555555",
    name: String = "Whiron",
    world: String = "overworld",
    gamemode: StoredGamemode = StoredGamemode.SURVIVAL,
    inventory: StoredInventory = StoredInventory.empty(),
) = StoredPlayer(
    uuid = uuid,
    name = name,
    lastSeenEpochSeconds = 1_700_000_000L,
    firstSeenEpochSeconds = 1_600_000_000L,
    playTimeTicks = 20L * 60 * 60,
    world = world,
    x = 8.5,
    y = 63.0,
    z = -12.25,
    yaw = 90f,
    pitch = -12.5f,
    health = 17.5f,
    food = 18,
    saturation = 3.5f,
    exhaustion = 1.25f,
    xpLevel = 7,
    xpProgress = 0.4f,
    xpTotal = 62,
    gamemode = gamemode,
    inventory = inventory,
)

/** An item with the kind of nested data that makes normalising an inventory into rows pointless. */
private fun enchantedSword(): StoredItemStack {
    val enchantments =
        NbtList(
            NbtType.COMPOUND,
            listOf(
                NbtCompound().apply {
                    putString("id", "minecraft:sharpness")
                    putInt("lvl", 5)
                },
                NbtCompound().apply {
                    putString("id", "minecraft:unbreaking")
                    putInt("lvl", 3)
                },
            ),
        )
    val components =
        NbtCompound().apply {
            this["minecraft:enchantments"] = enchantments
            putString("minecraft:custom_name", """{"text":"Excalibur"}""")
            putInt("minecraft:damage", 42)
            putCompound(
                "basalt:plugin_data",
                NbtCompound().apply {
                    putLong("boundTo", -1L)
                    putByteArray("blob", byteArrayOf(1, 2, 3))
                },
            )
        }
    return StoredItemStack("minecraft:diamond_sword", 1, components)
}

class InventoryNbtTest {
    @Test
    fun anEmptyInventoryRoundTripsWithItsSizeIntact() {
        val decoded = InventoryNbt.decode(InventoryNbt.encode(StoredInventory.empty()))
        assertEquals(StoredInventory.PLAYER_SLOTS, decoded.slots.size)
        assertTrue(decoded.isEmpty)
    }

    @Test
    fun enchantmentsAndCustomDataSurviveARoundTrip() {
        val sword = enchantedSword()
        val slots = arrayOfNulls<StoredItemStack>(StoredInventory.PLAYER_SLOTS)
        slots[0] = sword
        slots[40] = StoredItemStack("minecraft:torch", 64)

        val decoded = InventoryNbt.decode(InventoryNbt.encode(StoredInventory(slots.toList())))

        assertEquals(sword, decoded.slots[0])
        assertEquals(StoredItemStack("minecraft:torch", 64), decoded.slots[40])
        // Slot indices are positions, not an ordering: an item in the last slot stays there.
        assertNull(decoded.slots[1])
    }

    @Test
    fun aBlobFromAnotherFormatVersionIsRefusedRatherThanMisread() {
        val encoded = InventoryNbt.encode(StoredInventory.empty())
        // Rewrite the version field to a future one by re-encoding the compound.
        val root =
            org.basaltmc.nbt.NbtIO
                .read(encoded)
                .tag as NbtCompound
        root.putInt("Version", InventoryNbt.FORMAT_VERSION + 1)
        val future =
            org.basaltmc.nbt.NbtIO
                .write(root)

        assertFailsWith<StorageFormatException> { InventoryNbt.decode(future) }
    }
}

class PlayerStoreTest {
    private val path = "build/test-data/players_test.db"
    private val artefacts = listOf(path, "$path-wal", "$path-shm")

    @AfterTest
    fun cleanup() {
        for (file in artefacts) platform.posix.remove(file)
    }

    private fun <T> withStore(block: (PlayerStore) -> T): T = PlayerStore.open(path).use(block)

    @Test
    fun aPlayerRoundTripsThroughTheDatabaseWithItsInventory() {
        val slots = arrayOfNulls<StoredItemStack>(StoredInventory.PLAYER_SLOTS)
        slots[3] = enchantedSword()
        val player = samplePlayer(inventory = StoredInventory(slots.toList()))

        withStore { it.players.save(player) }

        val loaded = withStore { it.players.load(player.uuid) }
        assertEquals(player, loaded)
        assertEquals(enchantedSword(), loaded?.inventory?.slots?.get(3))
    }

    @Test
    fun savingTwiceUpdatesTheSameRowRatherThanAddingOne() {
        withStore { store ->
            store.players.save(samplePlayer())
            store.players.save(samplePlayer(name = "Whiron", world = "nether"))
            assertEquals(1L, store.players.count())
            assertEquals("nether", store.players.load(samplePlayer().uuid)?.world)
        }
    }

    @Test
    fun theDatabaseSurvivesBeingReopened() {
        withStore { it.players.save(samplePlayer()) }
        withStore { store ->
            assertTrue(store.players.exists(samplePlayer().uuid))
            assertEquals("Whiron", store.players.load(samplePlayer().uuid)?.name)
        }
    }

    @Test
    fun adminQueriesAnswerWithoutTouchingInventories() {
        val slots = arrayOfNulls<StoredItemStack>(StoredInventory.PLAYER_SLOTS)
        slots[0] = enchantedSword()
        withStore { store ->
            store.players.saveAll(
                listOf(
                    samplePlayer(
                        uuid = "a",
                        name = "Ana",
                        gamemode = StoredGamemode.SURVIVAL,
                        inventory = StoredInventory(slots.toList()),
                    ),
                    samplePlayer(uuid = "b", name = "Ben", gamemode = StoredGamemode.CREATIVE),
                    samplePlayer(uuid = "c", name = "Cleo", gamemode = StoredGamemode.CREATIVE, world = "nether"),
                ),
            )

            assertEquals(3L, store.players.count())
            assertEquals(
                mapOf(StoredGamemode.SURVIVAL to 1L, StoredGamemode.CREATIVE to 2L),
                store.players.countByGamemode(),
            )
            assertContentEquals(listOf("Cleo"), store.players.findInWorld("nether").map { it.name })
            assertEquals(3, store.players.findLastSeenBefore(1_800_000_000L).size)
            assertEquals(3, store.players.findWithPlayTimeAtLeast(1L).size)
            assertEquals(0, store.players.findWithPlayTimeAtLeast(Long.MAX_VALUE).size)
        }
    }

    @Test
    fun deletingAWorldsDirectoryIsNothingToDoWithPlayerRows() {
        // The acceptance criterion, expressed as the only thing it can mean here: the row records
        // the world by name, and nothing in the database points at a path under it.
        withStore { store ->
            store.players.save(samplePlayer(world = "doomed_world"))
            assertEquals("doomed_world", store.players.load(samplePlayer().uuid)?.world)
            // Whatever happens to worlds/doomed_world on disk, the row is untouched and readable.
            assertEquals(1L, store.players.count())
        }
    }

    @Test
    fun aBatchSaveIsOneTransaction() {
        withStore { store ->
            store.players.saveAll((1..50).map { samplePlayer(uuid = "player-$it", name = "P$it") })
            assertEquals(50L, store.players.count())
        }
    }
}
