package org.basaltmc.server.runtime.plugins

import org.basaltmc.server.plugins.PluginHost
import org.basaltmc.server.plugins.PluginSurface
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.WordArgument
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.TextColor

/**
 * `/plugins`, which is where an operator finds out what is loaded and takes something in or out
 * without stopping the server.
 *
 * Not a vanilla command, so it lives here rather than among them, and it is the one place the three
 * hot operations are reachable from: plug in, reload, unplug.
 */
public class PluginsCommand(
    private val host: PluginHost,
) : Command(name = "plugins", permissionLevel = PermissionLevel.ADMINS) {
    override fun build(root: CommandNodeBuilder) {
        root.executes { context ->
            val statuses = host.statuses
            if (statuses.isEmpty()) {
                context.success(Component.text("No plugins are loaded.", TextColor.GRAY))
                return@executes 0
            }
            context.success(Component.text("${statuses.size} plugin(s):", TextColor.GRAY))
            for (status in statuses) {
                val state =
                    when {
                        status.disabled -> Component.text(" off after ${status.errorStreak} in a row", TextColor.RED)
                        status.errors > 0 -> Component.text(" ${status.errors} error(s)", TextColor.YELLOW)
                        else -> Component.text(" ok", TextColor.GREEN)
                    }
                context.success(
                    Component
                        .text("  ${status.name} ${status.version}", TextColor.WHITE)
                        .append(
                            Component.text(
                                " ${status.registrations} registration(s)" +
                                    (if (status.editable) ", folder" else ""),
                                TextColor.GRAY,
                            ),
                        ).append(state),
                )
            }
            statuses.size
        }

        root.literal("surfaces") {
            executes { context ->
                val claims = host.surfaces.all()
                if (claims.isEmpty()) {
                    context.success(Component.text("No plugin is drawing anything.", TextColor.GRAY))
                    return@executes 0
                }
                for (surface in PluginSurface.entries) {
                    val onIt = claims.filter { it.surface == surface }
                    if (onIt.isEmpty()) continue
                    context.success(Component.text(surface.surfaceName, TextColor.GRAY))
                    for ((index, claim) in onIt.withIndex()) {
                        // First is whoever wins with no conditions in play; a `when` can still move
                        // it per player, and saying so is better than pretending the list is final.
                        val standing =
                            when {
                                claim.yielded -> Component.text(" let go", TextColor.RED)
                                index == 0 -> Component.text(" draws it", TextColor.GREEN)
                                else -> Component.text(" waiting", TextColor.YELLOW)
                            }
                        context.success(
                            Component
                                .text("  ${claim.pluginId}", TextColor.WHITE)
                                .append(Component.text(" priority ${claim.priority}", TextColor.GRAY))
                                .append(standing)
                                .append(
                                    if (claim.appliesTo != null) {
                                        Component.text(" (only sometimes)", TextColor.GRAY)
                                    } else {
                                        Component.empty()
                                    },
                                ),
                        )
                    }
                }
                claims.size
            }
        }

        root.literal("packets") {
            executes { context ->
                val names = PacketNames.all()
                context.success(
                    Component.text("${names.size} packet(s) a plugin can name:", TextColor.GRAY),
                )
                // Grouped by direction, because that is how a plugin looks for one.
                for (side in listOf("serverbound", "clientbound")) {
                    val theirs = names.filter { it.startsWith("$side.") }.map { it.substringAfter('.') }
                    context.success(Component.text("  $side (${theirs.size})", TextColor.WHITE))
                    context.success(Component.text("    ${theirs.joinToString(", ")}", TextColor.GRAY))
                }
                names.size
            }
        }

        root.literal("load") {
            executes { context ->
                val report = host.loadAll()
                if (report.loaded.isEmpty()) {
                    context.success(Component.text("Nothing new to load.", TextColor.GRAY))
                } else {
                    context.success(Component.text("Loaded ${report.loaded.joinToString(", ")}.", TextColor.GREEN))
                }
                for ((what, why) in report.refused) {
                    context.success(Component.text("$what did not load: $why", TextColor.RED))
                }
                report.loaded.size
            }
        }

        root.literal("reload") {
            argument("plugin", WordArgument) {
                executes { context ->
                    val id = context.get<String>("plugin")
                    if (!host.isLoaded(id)) {
                        context.success(Component.text("'$id' is not loaded.", TextColor.RED))
                        return@executes 0
                    }
                    val report = host.reload(id)
                    if (id in report.loaded) {
                        context.success(Component.text("Reloaded $id.", TextColor.GREEN))
                        1
                    } else {
                        val why = report.refused.firstOrNull { it.first == id }?.second ?: "it did not come back"
                        context.success(Component.text("$id is now unloaded: $why", TextColor.RED))
                        0
                    }
                }
            }
        }

        root.literal("unload") {
            argument("plugin", WordArgument) {
                executes { context ->
                    val id = context.get<String>("plugin")
                    val gone = host.unload(id)
                    if (gone.isEmpty()) {
                        context.success(Component.text("'$id' is not loaded.", TextColor.RED))
                        0
                    } else {
                        context.success(Component.text("Unloaded ${gone.joinToString(", ")}.", TextColor.GREEN))
                        gone.size
                    }
                }
            }
        }
    }
}
