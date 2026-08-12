package org.basaltmc.server.runtime.ui

import org.basaltmc.server.runtime.config.BossBarConfig
import org.basaltmc.server.runtime.config.OnceReporter
import org.basaltmc.server.runtime.config.ScoreboardConfig
import org.basaltmc.server.runtime.config.TabListConfig
import org.basaltmc.server.runtime.config.selectProfile
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.common.placeholder.PlaceholderContext
import org.basaltmc.wire.common.placeholder.PlaceholderRegistry
import org.basaltmc.wire.common.placeholder.PlaceholderText
import org.basaltmc.wire.common.placeholder.PlaceholderValue
import org.basaltmc.wire.protocol.play.BossBarColor
import org.basaltmc.wire.protocol.play.BossBarOverlay
import org.basaltmc.wire.protocol.play.BossEventPacket
import org.basaltmc.wire.protocol.play.DisplaySlot
import org.basaltmc.wire.protocol.play.NumberFormat
import org.basaltmc.wire.protocol.play.ResetScorePacket
import org.basaltmc.wire.protocol.play.SetDisplayObjectivePacket
import org.basaltmc.wire.protocol.play.SetObjectivePacket
import org.basaltmc.wire.protocol.play.SetScorePacket
import org.basaltmc.wire.protocol.play.TabListPacket
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.MiniMessage
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/** Everything the four UI files add up to, handed to a session as one thing. */
public class UiConfigs(
    public val tabList: TabListConfig = TabListConfig.DISABLED,
    public val scoreboard: ScoreboardConfig = ScoreboardConfig.DISABLED,
    public val bossBar: BossBarConfig = BossBarConfig.DISABLED,
) {
    public val anyEnabled: Boolean get() = tabList.enabled || scoreboard.enabled || bossBar.enabled
}

/**
 * The tab list, scoreboard and boss bar of one player.
 *
 * Lives on that player's own connection thread, driven from the same loop that already ticks hunger,
 * because everything here writes to a socket and the shared tick thread must never do that: one
 * client that stops reading would otherwise park saving, chunk unloading and every other player.
 *
 * Every piece is redrawn on its own interval and **sent only when the text it renders to changes**.
 * That is not a micro-optimisation: at 20 refreshes a second a scoreboard is a dozen packets per
 * player per tick, and almost every one of them would say exactly what the last one said.
 */
@OptIn(ExperimentalUuidApi::class)
public class PlayerUi(
    private val configs: () -> UiConfigs,
    private val placeholders: PlaceholderRegistry,
    private val send: (packetId: Int, write: (org.basaltmc.wire.io.ByteBuffer) -> Unit) -> Unit,
    private val logger: Logger = Logger("Ui"),
    private val reporter: OnceReporter = OnceReporter(),
) {
    private var lastTabList: Pair<String, String>? = null
    private var lastScoreboardTitle: String? = null
    private var lastScoreboardLines: List<String> = emptyList()
    private var scoreboardShown = false
    private var bossBarId: Uuid? = null
    private var lastBossText: String? = null
    private var lastBossProgress: Float? = null
    private var lastBossStyle: Pair<BossBarColor, BossBarOverlay>? = null

    /**
     * Called every tick from the connection's own loop. [tick] only decides which pieces are due.
     *
     * Takes a context rather than a player because nothing in here needs to know what a player is:
     * it renders whatever the placeholders resolve to, which is also what makes it testable without
     * a socket and a login behind it.
     */
    public fun tick(
        tick: Long,
        context: PlaceholderContext,
    ) {
        if (due(tick, configs().tabList.enabled, configs().tabList.updateIntervalTicks)) updateTabList(context)
        if (due(tick, configs().scoreboard.enabled, configs().scoreboard.updateIntervalTicks)) updateScoreboard(context)
        if (due(tick, configs().bossBar.enabled, configs().bossBar.updateIntervalTicks)) updateBossBar(context)
    }

    /** Takes down anything this player was shown. Called when they leave or change worlds. */
    public fun clear() {
        if (scoreboardShown) {
            send(SetObjectivePacket.ID) {
                SetObjectivePacket(OBJECTIVE, SetObjectivePacket.METHOD_REMOVE).write(it)
            }
            scoreboardShown = false
            lastScoreboardLines = emptyList()
            lastScoreboardTitle = null
        }
        bossBarId?.let { id ->
            send(BossEventPacket.ID) { BossEventPacket(id, BossEventPacket.Operation.Remove).write(it) }
            bossBarId = null
            lastBossText = null
            lastBossProgress = null
            lastBossStyle = null
        }
    }

    /**
     * An interval of 0 means "once, on the first tick": some panels are static, and re-sending a
     * fixed header twenty times a second is pure waste.
     */
    private fun due(
        tick: Long,
        enabled: Boolean,
        interval: Int,
    ): Boolean {
        if (!enabled) return false
        if (interval <= 0) return tick == 0L
        return tick % interval == 0L
    }

    private fun updateTabList(context: PlaceholderContext) {
        val profile =
            configs()
                .tabList.profiles
                .selectProfile(
                    configs().tabList.defaultProfile,
                    placeholders,
                    context,
                    logger,
                    reporter,
                    "tab list",
                ).inheriting(configs().tabList.defaultProfile)
        val header = render(profile.headerLines.orEmpty(), context)
        val footer = render(profile.footerLines.orEmpty(), context)
        if (lastTabList == header.first to footer.first) return
        lastTabList = header.first to footer.first
        send(TabListPacket.ID) { TabListPacket(header.second, footer.second).write(it) }
    }

    private fun updateScoreboard(context: PlaceholderContext) {
        val profile =
            configs()
                .scoreboard.profiles
                .selectProfile(
                    configs().scoreboard.defaultProfile,
                    placeholders,
                    context,
                    logger,
                    reporter,
                    "scoreboard",
                ).inheriting(configs().scoreboard.defaultProfile)

        val title = substitute(profile.title.orEmpty(), context)
        val lines =
            profile.lines
                .orEmpty()
                .take(ScoreboardConfig.MAX_LINES)
                .map { substitute(it, context) }

        if (!scoreboardShown) {
            send(SetObjectivePacket.ID) {
                SetObjectivePacket(
                    objectiveName = OBJECTIVE,
                    method = SetObjectivePacket.METHOD_ADD,
                    displayName = MiniMessage.parse(title),
                    numberFormat = NumberFormat.Blank,
                ).write(it)
            }
            send(SetDisplayObjectivePacket.ID) {
                SetDisplayObjectivePacket(DisplaySlot.SIDEBAR, OBJECTIVE).write(it)
            }
            scoreboardShown = true
            lastScoreboardTitle = title
        } else if (title != lastScoreboardTitle) {
            send(SetObjectivePacket.ID) {
                SetObjectivePacket(
                    objectiveName = OBJECTIVE,
                    method = SetObjectivePacket.METHOD_CHANGE,
                    displayName = MiniMessage.parse(title),
                    numberFormat = NumberFormat.Blank,
                ).write(it)
            }
            lastScoreboardTitle = title
        }

        for ((index, line) in lines.withIndex()) {
            if (lastScoreboardLines.getOrNull(index) == line) continue
            val rendered = MiniMessage.parse(line)
            // The score orders the rows, highest at the top, so it counts down from the line count.
            val score = lines.size - index
            send(SetScorePacket.ID) {
                SetScorePacket(owner(index), OBJECTIVE, score, rendered, NumberFormat.Blank).write(it)
            }
        }
        // A board that got shorter has to have its leftovers taken away, or they stay on screen.
        for (index in lines.size until lastScoreboardLines.size) {
            send(ResetScorePacket.ID) { ResetScorePacket(owner(index), OBJECTIVE).write(it) }
        }
        lastScoreboardLines = lines
    }

    private fun updateBossBar(context: PlaceholderContext) {
        val profile =
            configs()
                .bossBar.profiles
                .selectProfile(
                    configs().bossBar.defaultProfile,
                    placeholders,
                    context,
                    logger,
                    reporter,
                    "boss bar",
                ).inheriting(configs().bossBar.defaultProfile)

        val text = substitute(profile.text.orEmpty(), context)
        val progress = progressOf(profile.progress, context)
        val color = profile.color ?: BossBarColor.WHITE
        val overlay = profile.overlay ?: BossBarOverlay.PROGRESS

        val id = bossBarId
        if (id == null) {
            val fresh = Uuid.random()
            bossBarId = fresh
            send(BossEventPacket.ID) {
                BossEventPacket(
                    fresh,
                    BossEventPacket.Operation.Add(MiniMessage.parse(text), progress, color, overlay),
                ).write(it)
            }
        } else {
            // Nudged rather than re-added: re-adding restarts the client's own fade, which reads as
            // a flicker every time anything on the bar changes.
            if (text != lastBossText) {
                send(BossEventPacket.ID) {
                    BossEventPacket(id, BossEventPacket.Operation.UpdateName(MiniMessage.parse(text))).write(it)
                }
            }
            if (progress != lastBossProgress) {
                send(BossEventPacket.ID) {
                    BossEventPacket(id, BossEventPacket.Operation.UpdateProgress(progress)).write(it)
                }
            }
            if (color to overlay != lastBossStyle) {
                send(BossEventPacket.ID) {
                    BossEventPacket(id, BossEventPacket.Operation.UpdateStyle(color, overlay)).write(it)
                }
            }
        }
        lastBossText = text
        lastBossProgress = progress
        lastBossStyle = color to overlay
    }

    /** A number between 0 and 1, however the operator wrote it: a literal, or anything resolving to one. */
    private fun progressOf(
        raw: String?,
        context: PlaceholderContext,
    ): Float {
        val text = substitute(raw ?: "1.0", context)
        val value = text.toFloatOrNull() ?: return 1f
        return value.coerceIn(0f, 1f)
    }

    /** The substituted text (for comparing against last time) and the parsed component (for sending). */
    private fun render(
        lines: List<String>,
        context: PlaceholderContext,
    ): Pair<String, Component> {
        if (lines.isEmpty()) return "" to Component.empty()
        val text = lines.joinToString("\n") { substitute(it, context) }
        var component = MiniMessage.parse(text.substringBefore("\n"))
        for (line in text.split("\n").drop(1)) {
            component = component.append(Component.text("\n")).append(MiniMessage.parse(line))
        }
        return text to component
    }

    private fun substitute(
        text: String,
        context: PlaceholderContext,
    ): String =
        PlaceholderText.substitute(text, placeholders, context) { name, value ->
            if (value is PlaceholderValue.Unknown) {
                reporter.report(logger, "A UI file uses %$name%, which is not a placeholder this server knows.")
                null
            } else {
                // Ordinary: a panel that shows something only some players have should go blank for
                // the rest, not show them the name of the placeholder.
                ""
            }
        }

    private fun owner(index: Int): String = "basalt.line.$index"

    private companion object {
        /** One objective per player connection; the name only has to be unique within that client. */
        const val OBJECTIVE = "basalt"
    }
}
