package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.IntegerArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.wire.text.Component

/**
 * parity: `SetPlayerIdleTimeoutCommand`.
 *
 * The value is recorded and reported; nothing kicks an idle player yet, because idleness is
 * measured from movement and input the server does not track per player. Stated here rather than
 * accepted silently.
 */
public class SetIdleTimeoutCommand(
    private val setTimeoutMinutes: (Int) -> Unit,
) : Command(name = "setidletimeout", permissionLevel = PermissionLevel.ADMINS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("minutes", IntegerArgument(min = 0)) {
            executes { context ->
                val minutes = context.get<Int>("minutes")
                setTimeoutMinutes(minutes)
                context.success(
                    if (minutes > 0) {
                        Component.translatable(
                            "commands.setidletimeout.success",
                            Component.text(minutes.toString()),
                        )
                    } else {
                        Component.translatable("commands.setidletimeout.success.disabled")
                    },
                    broadcastToAdmins = true,
                )
                minutes
            }
        }
    }
}
