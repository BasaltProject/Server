package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.GameModeArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.PlayersArgument
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.wire.protocol.play.Gamemode
import org.basaltmc.wire.text.Component

/** parity: `GameModeCommand`. */
public class GameModeCommand : Command(name = "gamemode", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("gamemode", GameModeArgument) {
            executes { context -> setMode(context, listOf(context.requirePlayer()), context.get("gamemode")) }
            argument("target", PlayersArgument(single = false)) {
                executes { context -> setMode(context, context.get("target"), context.get("gamemode")) }
            }
        }
    }

    /**
     * parity: `GameModeCommand.logGamemodeChange`, including who is told what. Changing your own
     * mode reports once to you; changing someone else's tells them separately with
     * `gameMode.changed` and reports to you with their name.
     */
    private fun setMode(
        context: CommandContext,
        targets: List<PlayerSession>,
        mode: Gamemode,
    ): Int {
        var changed = 0
        val modeName = Component.translatable("gameMode.${mode.name.lowercase()}")
        for (target in targets) {
            if (target.gamemode == mode) continue
            target.connection.setGamemode(mode)
            changed++
            if (target.uuid == context.source.player?.uuid) {
                context.success(
                    Component.translatable("commands.gamemode.success.self", modeName),
                    broadcastToAdmins = true,
                )
            } else {
                target.sendSystemMessage(Component.translatable("gameMode.changed", modeName))
                context.success(
                    Component.translatable(
                        "commands.gamemode.success.other",
                        Component.text(target.name),
                        modeName,
                    ),
                    broadcastToAdmins = true,
                )
            }
        }
        return changed
    }
}
