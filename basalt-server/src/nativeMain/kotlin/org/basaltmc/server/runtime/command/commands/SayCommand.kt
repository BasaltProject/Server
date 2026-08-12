package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.MessageArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.wire.text.Component

/**
 * parity: `SayCommand`, whose output is the `say_command` chat type, rendered by
 * `chat.type.announcement`.
 */
public class SayCommand : Command(name = "say", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("message", MessageArgument) {
            executes { context ->
                context.environment.broadcast(
                    Component.translatable(
                        "chat.type.announcement",
                        context.source.displayName,
                        Component.text(context.get<String>("message")),
                    ),
                )
                1
            }
        }
    }
}
