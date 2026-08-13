package org.basaltmc.server.runtime.command

import org.basaltmc.server.runtime.command.commands.BossBarCommand
import org.basaltmc.server.runtime.command.commands.BossBarRegistry
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.protocol.play.BossBarColor
import org.basaltmc.wire.protocol.play.BossBarOverlay
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.PlainTextSerializer
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

private class Recorder : CommandSource {
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

/** `/bossbar` against its registry: nobody is online, so this is about the bars, not the packets. */
class BossBarCommandTest {
    private val bars = BossBarRegistry()

    private val dispatcher =
        CommandDispatcher(
            CommandEnvironment(
                players = PlayerRegistry(),
                groupsProvider = { testGroups() },
                worldSeed = 0L,
                maxPlayers = 20,
            ),
        ).also { it.register(BossBarCommand(bars)) }

    private val source = Recorder()

    private fun run(command: String): Int = dispatcher.execute(source, command)

    @Test
    fun addCreatesABarWithVanillaDefaults() {
        assertEquals(1, run("""bossbar add basalt:test {"text":"Test"}"""))
        val bar = checkNotNull(bars[Identifier.parse("basalt:test")])
        assertEquals(BossBarColor.WHITE, bar.color)
        assertEquals(BossBarOverlay.PROGRESS, bar.overlay)
        assertEquals(0, bar.value)
        assertEquals(100, bar.max)
        assertTrue(bar.visible)
    }

    @Test
    fun addRefusesAnIdThatAlreadyExists() {
        run("""bossbar add basalt:dup {"text":"a"}""")
        run("""bossbar add basalt:dup {"text":"b"}""")
        assertTrue(source.messages.any { "commands.bossbar.create.failed" in it })
    }

    @Test
    fun valueAndMaxDriveTheProgressTheClientWouldSee() {
        run("""bossbar add basalt:progress {"text":"p"}""")
        run("bossbar set basalt:progress max 200")
        assertEquals(50, run("bossbar set basalt:progress value 50"))
        assertEquals(0.25f, checkNotNull(bars[Identifier.parse("basalt:progress")]).progress)
    }

    @Test
    fun settingAValueTwiceIsRefusedAsUnchanged() {
        run("""bossbar add basalt:same {"text":"s"}""")
        run("bossbar set basalt:same value 10")
        run("bossbar set basalt:same value 10")
        assertTrue(source.messages.any { "commands.bossbar.set.value.unchanged" in it })
    }

    @Test
    fun styleAndColourAreTakenByTheirVanillaNames() {
        run("""bossbar add basalt:style {"text":"s"}""")
        run("bossbar set basalt:style color red")
        run("bossbar set basalt:style style notched_10")
        val bar = checkNotNull(bars[Identifier.parse("basalt:style")])
        assertEquals(BossBarColor.RED, bar.color)
        assertEquals(BossBarOverlay.NOTCHED_10, bar.overlay)
    }

    @Test
    fun removeForgetsTheBar() {
        run("""bossbar add basalt:gone {"text":"g"}""")
        run("bossbar remove basalt:gone")
        assertNull(bars[Identifier.parse("basalt:gone")])
    }

    @Test
    fun anUnknownBarIsRefusedByName() {
        run("bossbar get basalt:missing value")
        assertTrue(source.messages.any { "commands.bossbar.unknown" in it })
    }
}
