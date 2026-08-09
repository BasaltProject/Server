package org.basaltmc.server.ui

import org.basaltmc.common.placeholder.PlaceholderContext
import org.basaltmc.common.placeholder.PlaceholderRegistry
import org.basaltmc.common.placeholder.PlaceholderValue
import org.basaltmc.io.ByteBuffer
import org.basaltmc.protocol.play.BossEventPacket
import org.basaltmc.protocol.play.ResetScorePacket
import org.basaltmc.protocol.play.SetDisplayObjectivePacket
import org.basaltmc.protocol.play.SetObjectivePacket
import org.basaltmc.protocol.play.SetScorePacket
import org.basaltmc.protocol.play.TabListPacket
import org.basaltmc.server.config.BossBarConfig
import org.basaltmc.server.config.BossBarProfile
import org.basaltmc.server.config.ProfileHeader
import org.basaltmc.server.config.ScoreboardConfig
import org.basaltmc.server.config.ScoreboardProfile
import org.basaltmc.server.config.TabListConfig
import org.basaltmc.server.config.TabListProfile
import org.basaltmc.text.PlainTextSerializer
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/** Records what was sent, decoding it back so assertions are about packets and not about bytes. */
private class Recorder {
    val sent = mutableListOf<Pair<Int, ByteBuffer>>()

    fun send(
        id: Int,
        write: (ByteBuffer) -> Unit,
    ) {
        val buffer = ByteBuffer()
        write(buffer)
        sent += id to buffer
    }

    fun ids(): List<Int> = sent.map { it.first }

    fun count(id: Int): Int = sent.count { it.first == id }

    fun <T> decode(
        id: Int,
        read: (ByteBuffer) -> T,
    ): List<T> = sent.filter { it.first == id }.map { read(it.second) }

    fun clear(): Unit = sent.clear()
}

class PlayerUiTest {
    private var online = 1

    private val registry =
        PlaceholderRegistry().apply {
            register("basalt_server") { params, _ ->
                if (params == "online") PlaceholderValue.of(online) else PlaceholderValue.Unknown
            }
        }

    private fun header(name: String = "default") =
        ProfileHeader(name, 0, isDefault = true, isActive = false, emptyList())

    private fun tabList(
        headerLines: List<String>,
        footerLines: List<String>,
        interval: Int = 1,
    ): TabListConfig {
        val profile = TabListProfile(header(), headerLines, footerLines)
        return TabListConfig(true, interval, listOf(profile), profile)
    }

    private fun scoreboard(lines: List<String>): ScoreboardConfig {
        val profile = ScoreboardProfile(header(), title = "T", lines = lines)
        return ScoreboardConfig(true, 1, listOf(profile), profile)
    }

    private fun bossBar(
        text: String,
        progress: String = "1.0",
    ): BossBarConfig {
        val profile = BossBarProfile(header(), text = text, progress = progress)
        return BossBarConfig(true, 1, listOf(profile), profile)
    }

    @Test
    fun theTabListIsSentOnceAndNotResentWhileItSaysTheSameThing() {
        val recorder = Recorder()
        val ui = PlayerUi(UiConfigs(tabList = tabList(listOf("head"), listOf("foot"))), registry, recorder::send)

        repeat(3) { ui.tick(it.toLong(), PlaceholderContext.EMPTY) }

        // Redrawn every tick, sent once: at twenty a second the rest would all say the same thing.
        assertEquals(1, recorder.count(TabListPacket.ID))
    }

    @Test
    fun theTabListIsResentAsSoonAsItsTextChanges() {
        val recorder = Recorder()
        val config = tabList(listOf("head"), listOf("%basalt_server_online% online"))
        val ui = PlayerUi(UiConfigs(tabList = config), registry, recorder::send)

        ui.tick(0, PlaceholderContext.EMPTY)
        online = 2
        ui.tick(1, PlaceholderContext.EMPTY)

        val footers =
            recorder
                .decode(TabListPacket.ID, TabListPacket::read)
                .map { PlainTextSerializer.serialize(it.footer) }
        assertEquals(listOf("1 online", "2 online"), footers)
    }

    @Test
    fun theScoreboardIsCreatedPutInTheSidebarAndFilledIn() {
        val recorder = Recorder()
        val ui = PlayerUi(UiConfigs(scoreboard = scoreboard(listOf("a", "b"))), registry, recorder::send)

        ui.tick(0, PlaceholderContext.EMPTY)

        assertEquals(1, recorder.count(SetObjectivePacket.ID))
        assertEquals(1, recorder.count(SetDisplayObjectivePacket.ID))
        val scores = recorder.decode(SetScorePacket.ID, SetScorePacket::read)
        // Highest score at the top, so the first line written is the first line seen.
        assertEquals(listOf(2, 1), scores.map { it.score })
        assertEquals(listOf("a", "b"), scores.map { PlainTextSerializer.serialize(it.display!!) })
    }

    @Test
    fun onlyTheLinesThatChangedAreSentAgain() {
        val recorder = Recorder()
        val ui =
            PlayerUi(
                UiConfigs(scoreboard = scoreboard(listOf("static", "%basalt_server_online% online"))),
                registry,
                recorder::send,
            )

        ui.tick(0, PlaceholderContext.EMPTY)
        recorder.clear()
        online = 5
        ui.tick(1, PlaceholderContext.EMPTY)

        assertEquals(1, recorder.count(SetScorePacket.ID))
        val line = recorder.decode(SetScorePacket.ID, SetScorePacket::read).single()
        assertEquals("5 online", PlainTextSerializer.serialize(line.display!!))
    }

    @Test
    fun aBoardThatGotShorterHasItsLeftoverLinesTakenAway() {
        val recorder = Recorder()
        val lines = mutableListOf("a", "b", "c")
        val ui = PlayerUi(UiConfigs(scoreboard = scoreboard(lines)), registry, recorder::send)

        ui.tick(0, PlaceholderContext.EMPTY)
        lines.removeLast()
        recorder.clear()
        ui.tick(1, PlaceholderContext.EMPTY)

        // Without this the third row stays on screen for as long as the player is connected.
        assertEquals("basalt.line.2", recorder.decode(ResetScorePacket.ID, ResetScorePacket::read).single().owner)
    }

    @Test
    fun theBossBarIsAddedOnceAndThenNudged() {
        val recorder = Recorder()
        val ui = PlayerUi(UiConfigs(bossBar = bossBar("%basalt_server_online% online")), registry, recorder::send)

        ui.tick(0, PlaceholderContext.EMPTY)
        online = 9
        ui.tick(1, PlaceholderContext.EMPTY)

        val operations = recorder.decode(BossEventPacket.ID, BossEventPacket::read).map { it.operation }
        // Added once, then renamed: re-adding restarts the client's own fade and reads as a flicker.
        assertTrue(operations.first() is BossEventPacket.Operation.Add)
        assertTrue(operations.drop(1).all { it is BossEventPacket.Operation.UpdateName })
    }

    @Test
    fun clearingTakesDownWhatWasShown() {
        val recorder = Recorder()
        val ui =
            PlayerUi(
                UiConfigs(scoreboard = scoreboard(listOf("a")), bossBar = bossBar("x")),
                registry,
                recorder::send,
            )

        ui.tick(0, PlaceholderContext.EMPTY)
        recorder.clear()
        ui.clear()

        assertEquals(
            SetObjectivePacket.METHOD_REMOVE,
            recorder.decode(SetObjectivePacket.ID, SetObjectivePacket::read).single().method,
        )
        val bar = recorder.decode(BossEventPacket.ID, BossEventPacket::read).single()
        assertTrue(bar.operation is BossEventPacket.Operation.Remove)
    }

    @Test
    fun nothingIsSentForAPieceThatIsSwitchedOff() {
        val recorder = Recorder()
        val ui = PlayerUi(UiConfigs(), registry, recorder::send)

        ui.tick(0, PlaceholderContext.EMPTY)

        assertTrue(recorder.ids().isEmpty())
    }

    @Test
    fun anIntervalOfZeroSendsOnceAndThenNeverAgain() {
        val recorder = Recorder()
        val config = tabList(listOf("head"), listOf("%basalt_server_online%"), interval = 0)
        val ui = PlayerUi(UiConfigs(tabList = config), registry, recorder::send)

        ui.tick(0, PlaceholderContext.EMPTY)
        online = 7
        for (tick in 1L..40L) ui.tick(tick, PlaceholderContext.EMPTY)

        assertEquals(1, recorder.count(TabListPacket.ID))
    }
}
