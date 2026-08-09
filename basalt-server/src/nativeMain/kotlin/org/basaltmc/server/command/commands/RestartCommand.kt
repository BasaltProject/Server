package org.basaltmc.server.command.commands

import org.basaltmc.server.command.Command
import org.basaltmc.server.command.CommandNodeBuilder
import org.basaltmc.server.command.PermissionLevel
import org.basaltmc.text.Component
import org.basaltmc.text.TextColor

/**
 * `/restart`: stops this server and starts a fresh one **in the same process**.
 *
 * Not a re-exec and not a new process. The running instance winds itself down exactly as `/stop`
 * winds it down (players saved and disconnected, worlds flushed, database checkpointed and closed,
 * port released) and then another one is built from the configuration files as they stand. Nothing
 * is carried over, which is the point: the port is bound again, the worlds are loaded again, and
 * everything `/reload` has to refuse because it was decided at startup is decided again.
 *
 * Staying in the same process is what makes this work at all here: Kotlin/Native does not hand a
 * program its own `argv[0]`, and the calls that would reveal the executable's path are not in the
 * platform bindings on every target, so re-launching the binary would have been the fragile option
 * rather than the obvious one.
 *
 * Players are disconnected. There is no way around that: their connection belongs to the instance
 * being replaced, and a restart that kept it would be a reload wearing a different name.
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
