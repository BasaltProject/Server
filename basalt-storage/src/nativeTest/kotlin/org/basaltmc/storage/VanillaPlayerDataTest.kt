package org.basaltmc.storage

import org.basaltmc.io.Gzip
import org.basaltmc.nbt.NbtCompound
import org.basaltmc.nbt.NbtDouble
import org.basaltmc.nbt.NbtFloat
import org.basaltmc.nbt.NbtIO
import org.basaltmc.nbt.NbtList
import org.basaltmc.nbt.NbtType
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull

/**
 * The fixture is written here rather than shipped as a file: a real `playerdata/<uuid>.dat` is
 * Mojang's, and this project does not carry their bytes. The field names and slot encoding it uses
 * are the interesting part, and those are protocol-shaped facts, not content.
 */
private fun vanillaPlayerDat(): ByteArray {
    val sword =
        NbtCompound().apply {
            putByte("Slot", 0)
            putString("id", "minecraft:diamond_sword")
            putInt("count", 1)
            putCompound(
                "components",
                NbtCompound().apply { putInt("minecraft:damage", 12) },
            )
        }
    val helmet =
        NbtCompound().apply {
            // 100 is vanilla's first armour slot, not an inventory position.
            putByte("Slot", 100)
            putString("id", "minecraft:iron_helmet")
            putInt("count", 1)
        }
    val shield =
        NbtCompound().apply {
            putByte("Slot", (-106).toByte())
            putString("id", "minecraft:shield")
            putInt("count", 1)
        }
    val root =
        NbtCompound().apply {
            this["Pos"] = NbtList(NbtType.DOUBLE, listOf(NbtDouble(10.5), NbtDouble(64.0), NbtDouble(-3.25)))
            this["Rotation"] = NbtList(NbtType.FLOAT, listOf(NbtFloat(90f), NbtFloat(-12.5f)))
            putFloat("Health", 15f)
            putInt("foodLevel", 17)
            putFloat("foodSaturationLevel", 2.5f)
            putFloat("foodExhaustionLevel", 1.5f)
            putInt("XpLevel", 12)
            putFloat("XpP", 0.25f)
            putInt("XpTotal", 300)
            putInt("playerGameType", 1)
            this["Inventory"] = NbtList(NbtType.COMPOUND, listOf(sword, helmet, shield))
            this["EnderItems"] = NbtList(NbtType.COMPOUND, emptyList())
        }
    return NbtIO.writeCompressed(root, "", Gzip::compress)
}

class VanillaPlayerDataTest {
    private val imported =
        VanillaPlayerData.read(
            uuid = "11111111-2222-3333-4444-555555555555",
            name = "Whiron",
            gzippedBytes = vanillaPlayerDat(),
            decompress = Gzip::decompress,
            nowEpochSeconds = 1_700_000_000L,
            worldName = "world",
        )

    @Test
    fun scalarsMapOntoTheirNewNames() {
        assertEquals(10.5, imported.x)
        assertEquals(64.0, imported.y)
        assertEquals(-3.25, imported.z)
        assertEquals(90f, imported.yaw)
        assertEquals(-12.5f, imported.pitch)
        assertEquals(15f, imported.health)
        assertEquals(17, imported.food)
        assertEquals(2.5f, imported.saturation)
        assertEquals(12, imported.xpLevel)
        assertEquals(0.25f, imported.xpProgress)
        assertEquals(300, imported.xpTotal)
        assertEquals(StoredGamemode.CREATIVE, imported.gamemode)
        assertEquals("world", imported.world)
    }

    @Test
    fun vanillaSlotNumbersAreFoldedIntoAFlatLayout() {
        assertEquals("minecraft:diamond_sword", imported.inventory.slots[0]?.id)
        // Armour slot 100 becomes the first slot past the 36 main ones, the offhand the one after.
        assertEquals("minecraft:iron_helmet", imported.inventory.slots[36]?.id)
        assertEquals("minecraft:shield", imported.inventory.slots[40]?.id)
        assertNull(imported.inventory.slots[1])
    }

    @Test
    fun itemComponentsSurviveTheImportUntouched() {
        assertEquals(
            12,
            imported.inventory.slots[0]
                ?.components
                ?.getInt("minecraft:damage"),
        )
    }

    @Test
    fun anImportedPlayerCanBeSavedAndReadBack() {
        val path = "./basalt_import_test.db"
        PlayerStore.open(path).use { it.players.save(imported) }
        val loaded = PlayerStore.open(path).use { it.players.load(imported.uuid) }
        assertEquals(imported, loaded)
        for (file in listOf(path, "$path-wal", "$path-shm")) platform.posix.remove(file)
    }
}
