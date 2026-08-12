package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.GreedyStringArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.world.ImportException
import org.basaltmc.server.runtime.world.VanillaSaveImport
import org.basaltmc.wire.text.Component

/**
 * `import-players <ruta>`: one-way import of an existing vanilla server's players into
 * `players.db`.
 *
 * No vanilla counterpart, so no parity claim and no translation key: the messages are literal
 * because a client would have nothing to resolve them against. Owner level, because it writes to
 * every row in the database.
 *
 * The path is a greedy string on purpose. A save directory is very often somewhere with a space in
 * it, and quoting is the kind of thing an operator gets wrong once and then blames the server for.
 */
public class ImportPlayersCommand : Command(name = "import-players", permissionLevel = PermissionLevel.OWNERS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("path", GreedyStringArgument) {
            executes { context ->
                val repository =
                    context.environment.playerRepository
                        ?: throw CommandSyntaxException(Component.text("This server has no player database."))
                val path = context.get<String>("path").trim().trimEnd('/')
                val report =
                    try {
                        VanillaSaveImport.run(path, repository, worldName = context.environment.defaultWorldName)
                    } catch (e: ImportException) {
                        throw CommandSyntaxException(Component.text(e.message ?: "Import failed."))
                    }
                context.success(
                    Component.text(
                        "Imported ${report.imported} player(s) from $path " +
                            "(${report.skipped} had no data, ${report.failed} unreadable).",
                    ),
                    broadcastToAdmins = true,
                )
                report.imported
            }
        }
    }
}
