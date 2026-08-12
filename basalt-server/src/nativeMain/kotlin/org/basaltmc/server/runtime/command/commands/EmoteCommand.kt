package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.MessageArgument
import org.basaltmc.wire.text.Component

/** parity: `EmoteCommands`, the `emote_command` chat type (`chat.type.emote`). Available to everyone. */
public class EmoteCommand : Command(name = "me") {
    override fun build(root: CommandNodeBuilder) {
        root.argument("action", MessageArgument) {
            executes { context ->
                context.environment.broadcast(
                    Component.translatable(
                        "chat.type.emote",
                        context.source.displayName,
                        Component.text(context.get<String>("action")),
                    ),
                )
                1
            }
        }
    }
}
