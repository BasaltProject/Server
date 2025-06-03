package org.basaltmc.server.runtime.world

import org.basaltmc.server.runtime.TEST_DATA_ROOT
import org.basaltmc.server.storage.PlayerStore
import org.basaltmc.server.storage.StoredGamemode
import org.basaltmc.wire.io.FileIO
import org.basaltmc.wire.io.Gzip
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtDouble
import org.basaltmc.wire.nbt.NbtIO
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtType
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertNull

/**
 * Builds a miniature vanilla server directory and imports it. The fixture is written here rather
 * than shipped: a real save is Mojang's, and what matters is the shape (a `usercache.json` index
 * beside a `playerdata/` folder), which is a fact about their layout, not their content.
 */
class VanillaSaveImportTest {
    private val save = "$TEST_DATA_ROOT/vanilla_save"
    private val databasePath = "$TEST_DATA_ROOT/import_command.db"

    private val whironUuid = "11111111-2222-3333-4444-555555555555"
    private val ghostUuid = "99999999-8888-7777-6666-555555555555"

    private fun playerDat(gamemode: Int): ByteArray {
        val root =
            NbtCompound().apply {
                this["Pos"] = NbtList(NbtType.DOUBLE, listOf(NbtDouble(1.5), NbtDouble(70.0), NbtDouble(-2.5)))
                putFloat("Health", 12f)
                putInt("foodLevel", 15)
                putInt("playerGameType", gamemode)
            }
        return NbtIO.writeCompressed(root, "", Gzip::compress)
    }

    private fun writeSave() {
        FileIO.createDirectories("$save/playerdata")
        FileIO.writeBytes("$save/playerdata/$whironUuid.dat", playerDat(gamemode = 1))
        // In the cache but never joined: vanilla remembers every name it ever resolved.
        FileIO.writeText(
            "$save/usercache.json",
            """
            [
              {"uuid":"$whironUuid","name":"Whiron","expiresOn":"2026-09-08 12:00:00 +0200"},
              {"uuid":"$ghostUuid","name":"Ghost","expiresOn":"2026-09-08 12:00:00 +0200"}
            ]
            """.trimIndent(),
        )
    }

    @AfterTest
    fun cleanup() {
        for (suffix in listOf("", "-wal", "-shm")) platform.posix.remove("$databasePath$suffix")
        platform.posix.remove("$save/playerdata/$whironUuid.dat")
        platform.posix.remove("$save/usercache.json")
    }

    @Test
    fun importsThePlayersThatHaveDataAndSkipsTheOnesThatDoNot() {
        writeSave()
        val report =
            PlayerStore.open(databasePath).use { store ->
                VanillaSaveImport.run(save, store.players, worldName = "world")
            }

        assertEquals(1, report.imported)
        assertEquals(1, report.skipped)
        assertEquals(0, report.failed)
    }

    @Test
    fun theImportedPlayerKeepsWhatVanillaHadRecorded() {
        writeSave()
        val loaded =
            PlayerStore.open(databasePath).use { store ->
                VanillaSaveImport.run(save, store.players, worldName = "world")
                store.players.load(whironUuid)
            }

        // The name comes from usercache.json: the .dat file does not contain one, which is the
        // whole reason the import is driven by that index.
        assertEquals("Whiron", loaded?.name)
        assertEquals(StoredGamemode.CREATIVE, loaded?.gamemode)
        assertEquals(12f, loaded?.health)
        assertEquals(1.5, loaded?.x)
        assertEquals("world", loaded?.world)
    }

    @Test
    fun runningItTwiceOverwritesRatherThanDuplicating() {
        writeSave()
        val count =
            PlayerStore.open(databasePath).use { store ->
                VanillaSaveImport.run(save, store.players, worldName = "world")
                VanillaSaveImport.run(save, store.players, worldName = "world")
                store.players.count()
            }
        assertEquals(1L, count)
    }

    @Test
    fun anUnreadableDataFileCostsThatPlayerAndNoOneElse() {
        writeSave()
        FileIO.writeBytes("$save/playerdata/$whironUuid.dat", byteArrayOf(1, 2, 3, 4))

        val report =
            PlayerStore.open(databasePath).use { store ->
                val result = VanillaSaveImport.run(save, store.players, worldName = "world")
                assertNull(store.players.load(whironUuid))
                result
            }

        assertEquals(0, report.imported)
        assertEquals(1, report.failed)
    }

    @Test
    fun aDirectoryThatIsNotAVanillaServerIsRejectedUpFront() {
        assertFailsWith<ImportException> {
            PlayerStore.open(databasePath).use { store ->
                VanillaSaveImport.run("$TEST_DATA_ROOT/not_a_save", store.players, worldName = "world")
            }
        }
    }
}
