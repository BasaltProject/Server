package org.basaltmc.server.command.commands

import org.basaltmc.server.command.Command
import org.basaltmc.server.command.CommandContext
import org.basaltmc.server.command.CommandNodeBuilder
import org.basaltmc.server.command.CommandSyntaxException
import org.basaltmc.server.command.PermissionLevel
import org.basaltmc.server.config.LiveConfigs
import org.basaltmc.text.Component
import org.basaltmc.text.TextColor

/**
 * `/reload`: re-reads the configuration files that are read on every use.
 *
 * Bukkit taught everyone to distrust this command, and for a good reason: there it re-runs every
 * plugin's enable and disable while the rest of the server keeps holding references to the objects
 * it just threw away, so the failure is silent, arbitrary and turns up an hour later. That is a
 * consequence of reloading *code*, not of the idea of reloading.
 *
 * Here the line is a property of the code rather than a judgement: a file whose values are read
 * every time they are used can be swapped under a running server, and a file whose values were read
 * once at startup and turned into a bound socket, a thread pool or a loaded world cannot. The first
 * group is reloaded, the second is refused **by name**, and nothing is left half-applied because
 * every file is read before any of them is applied.
 */
public class ReloadCommand(
    private val live: LiveConfigs,
    private val onReloaded: () -> Unit,
) : Command(name = "reload", permissionLevel = PermissionLevel.ADMINS) {
    override fun build(root: CommandNodeBuilder) {
        root.executes { context -> reload(context) }
        root.literal("confirm") {
            executes { context -> reload(context) }
        }
    }

    private fun reload(context: CommandContext): Int {
        val next =
            try {
                // Every file first, then the swap: a reload that applied three files and gave up on
                // the fourth would leave the server in a state no file on disk describes.
                LiveConfigs.read(logger = context.environment.logger)
            } catch (e: Exception) {
                throw CommandSyntaxException(
                    Component.text(
                        "Nothing was reloaded: ${e.message ?: "a configuration file could not be read"}",
                    ),
                )
            }

        live.replace(next)
        onReloaded()

        context.success(
            Component.text("Reloaded ${LiveConfigs.RELOADABLE_FILES.size} configuration file(s)."),
            broadcastToAdmins = true,
        )
        context.source.sendSystemMessage(
            Component.text("  ${LiveConfigs.RELOADABLE_FILES.joinToString(", ")}", TextColor.GRAY),
        )
        // Said every time rather than only when one of them changed: an operator who edited
        // server.toml and saw "reloaded" would reasonably believe their edit had taken.
        context.source.sendSystemMessage(
            Component.text(
                "  server.toml and worlds.toml are NOT reloaded: the port is already bound, the " +
                    "worlds are already loaded, and applying half of either is worse than not " +
                    "applying it. Restart for those.",
                TextColor.GRAY,
            ),
        )
        return LiveConfigs.RELOADABLE_FILES.size
    }
}
