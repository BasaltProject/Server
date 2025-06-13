package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.ComponentArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.PlayersArgument
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.wire.text.Component

/** parity: `TellRawCommand`. The message is a component, so styles and events survive to the client. */
public class TellRawCommand : Command(name = "tellraw", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("targets", PlayersArgument(single = false)) {
            argument("message", ComponentArgument) {
                executes { context -> tell(context, context.get("targets"), context.get("message")) }
            }
        }
    }

    private fun tell(
        context: CommandContext,
        targets: List<PlayerSession>,
        message: Component,
    ): Int {
        for (target in targets) target.sendSystemMessage(message)
        return targets.size
    }
}
