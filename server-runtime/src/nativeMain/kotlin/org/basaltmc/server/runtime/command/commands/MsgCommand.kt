package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.MessageArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.PlayersArgument
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.wire.text.Component

/**
 * parity: `MsgCommand`, with vanilla's own two keys: the sender sees `commands.message.display.
 * outgoing`, each recipient `commands.message.display.incoming`, and the client italicises both.
 */
public class MsgCommand :
    Command(
        name = "msg",
        permissionLevel = PermissionLevel.ALL,
        aliases = listOf("tell", "w"),
    ) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("targets", PlayersArgument(single = false)) {
            argument("message", MessageArgument) {
                executes { context -> send(context, context.get("targets"), context.get("message")) }
            }
        }
    }

    private fun send(
        context: CommandContext,
        targets: List<PlayerSession>,
        message: String,
    ): Int {
        val body = Component.text(message)
        for (target in targets) {
            target.sendSystemMessage(
                Component.translatable("commands.message.display.incoming", context.source.displayName, body),
            )
            context.source.sendSystemMessage(
                Component.translatable(
                    "commands.message.display.outgoing",
                    Component.text(target.name),
                    body,
                ),
            )
        }
        return targets.size
    }
}
