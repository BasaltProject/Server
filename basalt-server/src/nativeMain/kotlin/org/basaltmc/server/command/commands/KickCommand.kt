package org.basaltmc.server.command.commands

import org.basaltmc.server.command.Command
import org.basaltmc.server.command.CommandContext
import org.basaltmc.server.command.CommandNodeBuilder
import org.basaltmc.server.command.MessageArgument
import org.basaltmc.server.command.PermissionLevel
import org.basaltmc.server.command.PlayersArgument
import org.basaltmc.server.player.PlayerSession
import org.basaltmc.text.Component

/** parity: `KickCommand`, whose default reason is `multiplayer.disconnect.kicked`. */
public class KickCommand : Command(name = "kick", permissionLevel = PermissionLevel.ADMINS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("targets", PlayersArgument(single = false)) {
            executes { context ->
                kick(context, context.get("targets"), Component.translatable("multiplayer.disconnect.kicked"))
            }
            argument("reason", MessageArgument) {
                executes { context ->
                    kick(context, context.get("targets"), Component.text(context.get<String>("reason")))
                }
            }
        }
    }

    private fun kick(
        context: CommandContext,
        targets: List<PlayerSession>,
        reason: Component,
    ): Int {
        for (target in targets) {
            target.connection.disconnect(reason)
            context.success(
                Component.translatable("commands.kick.success", Component.text(target.name), reason),
                broadcastToAdmins = true,
            )
        }
        return targets.size
    }
}
