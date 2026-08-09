package org.basaltmc.server.command.commands

import org.basaltmc.server.command.Command
import org.basaltmc.server.command.CommandNodeBuilder
import org.basaltmc.server.command.PermissionLevel
import org.basaltmc.text.Component

/** parity: `StopCommand`. */
public class StopCommand : Command(name = "stop", permissionLevel = PermissionLevel.OWNERS) {
    override fun build(root: CommandNodeBuilder) {
        root.executes { context ->
            context.success(Component.translatable("commands.stop.stopping"), broadcastToAdmins = true)
            context.environment.stopServer()
            1
        }
    }
}
