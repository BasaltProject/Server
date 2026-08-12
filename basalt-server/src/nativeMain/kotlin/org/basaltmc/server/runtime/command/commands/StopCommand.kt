package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.wire.text.Component

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
