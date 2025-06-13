package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.config.LiveConfigs
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.TextColor

/**
 * `/reload`: re-reads the configuration files that are read on every use. Bukkit's version is
 * distrusted for good reason, but that comes from reloading *code* while the server holds
 * references to what it threw away.
 *
 * Here the line is a property of the file: values read on every use can be swapped under a running
 * server, values already turned into a bound socket, a thread pool or a loaded world cannot. The
 * second group is refused by name, and every file is read before any is applied, so nothing is
 * left half-done.
 */
public class ReloadCommand(
    private val live: LiveConfigs,
    private val onReloaded: () -> Unit,
    /** Datapack functions, which vanilla's own `/reload` re-reads. Returns how many were loaded. */
    private val reloadFunctions: () -> Int = { 0 },
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
        val functionCount = reloadFunctions()

        context.success(
            Component.text(
                "Reloaded ${LiveConfigs.RELOADABLE_FILES.size} configuration file(s) and " +
                    "$functionCount function(s).",
            ),
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
