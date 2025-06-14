package org.basaltmc.server.runtime.command

import org.basaltmc.server.runtime.command.commands.BanCommand
import org.basaltmc.server.runtime.command.commands.BanListCommand
import org.basaltmc.server.runtime.command.commands.PardonCommand
import org.basaltmc.server.runtime.command.commands.WhitelistCommand
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.storage.BanKind
import org.basaltmc.server.storage.PlayerStore
import org.basaltmc.wire.auth.OfflineUuid
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.PlainTextSerializer
import kotlin.test.AfterTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi

private class AccessConsole : CommandSource {
    val messages: MutableList<String> = mutableListOf()

    override val name: String = "Server"
    override val displayName: Component = Component.text("Server")
    override val permissionLevel: Int = PermissionLevel.OWNERS
    override val position: Triple<Double, Double, Double> = Triple(0.0, 0.0, 0.0)
    override val player: PlayerSession? = null

    override fun hasPermission(node: String?): Boolean = true

    override fun sendSystemMessage(message: Component) {
        messages += PlainTextSerializer.serialize(message)
    }
}

@OptIn(ExperimentalUuidApi::class)
class AccessCommandsTest {
    private val databasePath = "build/test-data/access-commands/players.db"

    private val store: PlayerStore

    init {
        for (suffix in listOf("", "-wal", "-shm")) platform.posix.remove("$databasePath$suffix")
        store = PlayerStore.open(databasePath)
    }

    private var whitelistEnforced = false

    private val dispatcher =
        CommandDispatcher(
            CommandEnvironment(
                players = PlayerRegistry(),
                groupsProvider = { testGroups() },
                worldSeed = 0L,
                maxPlayers = 20,
                playerRepository = store.players,
                access = store.access,
            ),
        ).also { it.register(BanCommand()) }
            .also { it.register(BanListCommand()) }
            .also { it.register(PardonCommand(ip = false)) }
            .also { it.register(PardonCommand(ip = true)) }
            .also {
                it.register(
                    WhitelistCommand(
                        enforced = { whitelistEnforced },
                        setEnforced = { value -> whitelistEnforced = value },
                    ),
                )
            }

    private val source = AccessConsole()

    @AfterTest
    fun close() {
        store.close()
    }

    private fun run(command: String): Int = dispatcher.execute(source, command)

    @Test
    fun banningSomebodyWhoHasNeverJoinedStillRecordsTheirOfflineUuid() {
        assertEquals(1, run("ban Griefer being a nuisance"))
        val uuid = OfflineUuid.forUsername("Griefer").toString()
        val ban = assertNotNull(store.access.banOf(uuid, BanKind.PLAYER))
        assertEquals("being a nuisance", ban.reason)
        assertEquals("Server", ban.source)
    }

    @Test
    fun banningTwiceIsRefused() {
        run("ban Twice")
        run("ban Twice")
        assertTrue(source.messages.any { "commands.ban.failed" in it })
    }

    @Test
    fun pardonLiftsTheBanAndRefusesWhenThereIsNone() {
        run("ban Forgiven")
        assertEquals(1, run("pardon Forgiven"))
        assertNull(store.access.banOf(OfflineUuid.forUsername("Forgiven").toString(), BanKind.PLAYER))
        run("pardon Forgiven")
        assertTrue(source.messages.any { "commands.pardon.failed" in it })
    }

    @Test
    fun banListCountsWhatIsThere() {
        run("ban One")
        run("ban Two")
        assertEquals(2, run("banlist players"))
    }

    @Test
    fun whitelistAddsRemovesAndRefusesDuplicates() {
        assertEquals(1, run("whitelist add Allowed"))
        assertTrue(store.access.whitelist().any { it.name == "Allowed" })
        run("whitelist add Allowed")
        assertTrue(source.messages.any { "commands.whitelist.add.failed" in it })
        assertEquals(1, run("whitelist remove Allowed"))
        assertTrue(store.access.whitelist().isEmpty())
    }

    @Test
    fun whitelistOnAndOffFollowTheRunningFlag() {
        assertFalse(whitelistEnforced)
        run("whitelist on")
        assertTrue(whitelistEnforced)
        run("whitelist on")
        assertTrue(source.messages.any { "commands.whitelist.alreadyOn" in it })
        run("whitelist off")
        assertFalse(whitelistEnforced)
    }
}
