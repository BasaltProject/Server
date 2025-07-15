package org.basaltmc.server.runtime.plugins

import org.basaltmc.server.plugins.PluginHost
import org.basaltmc.server.plugins.PluginSurface
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.WordArgument
import org.basaltmc.wire.io.FileIO
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
    /** Where the player database lives, so a purge can copy it before it deletes anything. */
    private val databasePath: () -> String? = { null },
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

        root.literal("orphans") {
            executes { context -> listOrphans(context) }
        }

        root.literal("purge") {
            argument("plugin", WordArgument) {
                executes { context -> purge(context, dryRun = true) }
                literal("confirm") {
                    executes { context -> purge(context, dryRun = false) }
                }
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

    private fun listOrphans(context: CommandContext): Int {
        val orphans = host.orphanedData()
        if (orphans.isEmpty()) {
            context.success(Component.text("Every plugin with data here is installed.", TextColor.GRAY))
            return 0
        }
        context.success(Component.text("${orphans.size} plugin(s) left data behind:", TextColor.YELLOW))
        for ((id, rows) in orphans.entries.sortedByDescending { it.value }) {
            context.success(
                Component
                    .text("  $id", TextColor.WHITE)
                    .append(Component.text(" $rows row(s)", TextColor.GRAY))
                    .append(Component.text("   /plugins purge $id", TextColor.DARK_GRAY)),
            )
        }
        return orphans.size
    }

    /**
     * `/plugins purge <id>` says what would go; `/plugins purge <id> confirm` does it.
     *
     * Two commands rather than a flag, and the harmless one is the short one, so nobody deletes a
     * thousand players' rows by mistyping. The database is copied first: the only thing worse than
     * losing the data is losing it with nothing to go back to.
     */
    private fun purge(
        context: CommandContext,
        dryRun: Boolean,
    ): Int {
        val id = context.get<String>("plugin")
        val rows = host.orphanedData()[id]
        if (rows == null) {
            context.success(
                Component.text("No data here belongs to '$id'. /plugins orphans lists what does.", TextColor.RED),
            )
            return 0
        }
        if (dryRun) {
            context.success(
                Component
                    .text("$rows row(s) of '$id' would go. ", TextColor.YELLOW)
                    .append(Component.text("/plugins purge $id confirm", TextColor.WHITE))
                    .append(Component.text(" to do it.", TextColor.YELLOW)),
            )
            return rows.toInt()
        }
        val copy = backup()
        if (copy == null) {
            context.success(
                Component.text("The database could not be copied, so nothing was purged.", TextColor.RED),
            )
            return 0
        }
        val gone = host.purge(id)
        if (gone == null) {
            context.success(Component.text("'$id' is loaded. Take it out of plugins/ first.", TextColor.RED))
            return 0
        }
        context.success(
            Component
                .text("Purged $gone row(s) of '$id'. ", TextColor.GREEN)
                .append(Component.text("A copy of the database is at $copy.", TextColor.GRAY)),
        )
        return gone.toInt()
    }

    /** Copies the database beside itself, named for what is about to happen to it. */
    private fun backup(): String? {
        val path = databasePath() ?: return null
        val bytes = FileIO.readBytes(path) ?: return null
        val copy = "$path.before-purge"
        return runCatching {
            FileIO.writeBytes(copy, bytes)
            copy
        }.getOrNull()
    }
}
