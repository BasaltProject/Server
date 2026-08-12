package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.TextColor

/**
 * `/restart`: winds this server down exactly as `/stop` does, then builds another one from the
 * configuration files as they stand, **in the same process**. Nothing carries over, which is the
 * point: everything `/reload` has to refuse because it was decided at startup is decided again.
 *
 * Same process because Kotlin/Native does not hand a program its own `argv[0]`, and the calls that
 * would reveal the executable's path are missing from some targets, so re-exec is the fragile
 * option here. Players are disconnected: their connection belongs to the instance being replaced.
 */
public class RestartCommand(
    private val requestRestart: () -> Unit,
) : Command(name = "restart", permissionLevel = PermissionLevel.ADMINS) {
    override fun build(root: CommandNodeBuilder) {
        root.executes { context ->
            val online =
                context.environment.players
                    .snapshot()
                    .size
            context.success(
                Component.text(
                    if (online == 0) {
                        "Restarting."
                    } else {
                        "Restarting: $online player(s) will be disconnected and can reconnect in a moment."
                    },
                ),
                broadcastToAdmins = true,
            )
            context.environment.broadcast(
                Component.text("The server is restarting. You can reconnect in a moment.", TextColor.YELLOW),
            )
            // After the messages, since the first thing a restart does is close the socket they
            // would have gone out on.
            requestRestart()
            1
        }
    }
}
