package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.ComponentArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.PlayersArgument
import org.basaltmc.server.runtime.command.TimeArgument
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.wire.protocol.play.ClearTitlesPacket
import org.basaltmc.wire.protocol.play.SetActionBarTextPacket
import org.basaltmc.wire.protocol.play.SetSubtitleTextPacket
import org.basaltmc.wire.protocol.play.SetTitleTextPacket
import org.basaltmc.wire.protocol.play.SetTitlesAnimationPacket
import org.basaltmc.wire.text.Component

/**
 * parity: `TitleCommand`, all six forms.
 *
 * `clear` and `reset` differ the way vanilla makes them differ: clearing hides whatever is showing,
 * resetting also puts the fade timings back to their defaults.
 */
public class TitleCommand : Command(name = "title", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("targets", PlayersArgument(single = false)) {
            literal("clear") { executes { context -> clear(context, resetTimes = false) } }
            literal("reset") { executes { context -> clear(context, resetTimes = true) } }
            for (kind in listOf("title", "subtitle", "actionbar")) {
                literal(kind) {
                    argument("title", ComponentArgument) {
                        executes { context -> show(context, kind, context.get("title")) }
                    }
                }
            }
            literal("times") {
                argument("fadeIn", TimeArgument(min = 0)) {
                    argument("stay", TimeArgument(min = 0)) {
                        argument("fadeOut", TimeArgument(min = 0)) {
                            executes { context -> times(context) }
                        }
                    }
                }
            }
        }
    }

    private fun clear(
        context: CommandContext,
        resetTimes: Boolean,
    ): Int {
        val targets = context.get<List<PlayerSession>>("targets")
        for (target in targets) {
            target.connection.sendPlayPacket(ClearTitlesPacket.ID) { ClearTitlesPacket(resetTimes).write(it) }
        }
        report(context, targets, if (resetTimes) "reset" else "cleared")
        return targets.size
    }

    private fun show(
        context: CommandContext,
        kind: String,
        text: Component,
    ): Int {
        val targets = context.get<List<PlayerSession>>("targets")
        for (target in targets) {
            when (kind) {
                "title" ->
                    target.connection.sendPlayPacket(
                        SetTitleTextPacket.ID,
                    ) { SetTitleTextPacket(text).write(it) }
                "subtitle" ->
                    target.connection.sendPlayPacket(SetSubtitleTextPacket.ID) {
                        SetSubtitleTextPacket(text).write(it)
                    }
                else ->
                    target.connection.sendPlayPacket(SetActionBarTextPacket.ID) {
                        SetActionBarTextPacket(text).write(it)
                    }
            }
        }
        report(context, targets, "show.$kind")
        return targets.size
    }

    private fun times(context: CommandContext): Int {
        val targets = context.get<List<PlayerSession>>("targets")
        val packet =
            SetTitlesAnimationPacket(
                fadeIn = context.get("fadeIn"),
                stay = context.get("stay"),
                fadeOut = context.get("fadeOut"),
            )
        for (target in targets) {
            target.connection.sendPlayPacket(SetTitlesAnimationPacket.ID) { packet.write(it) }
        }
        report(context, targets, "times")
        return targets.size
    }

    private fun report(
        context: CommandContext,
        targets: List<PlayerSession>,
        key: String,
    ) {
        context.success(
            if (targets.size == 1) {
                Component.translatable("commands.title.$key.single", Component.text(targets.single().name))
            } else {
                Component.translatable("commands.title.$key.multiple", Component.text(targets.size.toString()))
            },
            broadcastToAdmins = true,
        )
    }
}
