package org.basaltmc.server.command.commands

import org.basaltmc.server.command.Command
import org.basaltmc.server.command.CommandContext
import org.basaltmc.server.command.CommandNodeBuilder
import org.basaltmc.server.command.CommandSyntaxException
import org.basaltmc.server.command.GameProfileArgument
import org.basaltmc.server.command.PermissionLevel
import org.basaltmc.text.Component

/**
 * parity: `OpCommand`, except for where the list lives: vanilla keeps operators in `ops.json`,
 * Basalt keeps them in `basalt.toml` (one configuration file, by design). The command, its
 * permission level and its messages are unchanged.
 */
public class OpCommand : Command(name = "op", permissionLevel = PermissionLevel.ADMINS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("targets", GameProfileArgument) {
            executes { context ->
                val name = context.get<String>("targets")
                if (!context.environment.operators.add(name)) {
                    throw CommandSyntaxException(Component.translatable("commands.op.failed"))
                }
                resendCommandTree(context, name)
                context.success(
                    Component.translatable("commands.op.success", Component.text(name)),
                    broadcastToAdmins = true,
                )
                1
            }
        }
    }
}

/**
 * parity: `DeOpCommands`. Separate class, separate command, same list.
 */
public class DeopCommand : Command(name = "deop", permissionLevel = PermissionLevel.ADMINS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("targets", GameProfileArgument) {
            executes { context ->
                val name = context.get<String>("targets")
                if (!context.environment.operators.remove(name)) {
                    throw CommandSyntaxException(Component.translatable("commands.deop.failed"))
                }
                resendCommandTree(context, name)
                context.success(
                    Component.translatable("commands.deop.success", Component.text(name)),
                    broadcastToAdmins = true,
                )
                1
            }
        }
    }
}

/**
 * The tree a client holds is filtered by permission, so it is stale the moment that permission
 * changes: vanilla re-sends it from `sendPlayerPermissionLevel` for the same reason.
 */
private fun resendCommandTree(
    context: CommandContext,
    playerName: String,
) {
    context.environment.players
        .snapshot()
        .firstOrNull { it.name.equals(playerName, ignoreCase = true) }
        ?.connection
        ?.sendCommandTree()
}
