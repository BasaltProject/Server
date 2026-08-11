package org.basaltmc.server.command.commands

import org.basaltmc.server.command.Command
import org.basaltmc.server.command.CommandContext
import org.basaltmc.server.command.CommandNodeBuilder
import org.basaltmc.server.command.CommandSyntaxException
import org.basaltmc.server.command.GameProfileArgument
import org.basaltmc.server.command.PermissionLevel
import org.basaltmc.server.command.WordArgument
import org.basaltmc.text.Component

/**
 * parity: `OpCommand`, except for where membership lives.
 *
 * Vanilla keeps operators in `ops.json`; Basalt keeps which group a player is in as a column in
 * `players.db`, because the server writes it and only the server writes it. `groups.toml` says what
 * the operator group *is*; this says who is in it. The previous arrangement had the list in
 * `config/server.toml` and a function that rewrote that line of the file on every `op`, which was
 * the symptom of it being on the wrong side of that split.
 */
public class OpCommand : Command(name = "op", permissionLevel = PermissionLevel.ADMINS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("targets", GameProfileArgument) {
            executes { context ->
                val name = context.get<String>("targets")
                assign(context, name, context.environment.groups.operatorGroup.name)
                context.success(
                    Component.translatable("commands.op.success", Component.text(name)),
                    broadcastToAdmins = true,
                )
                1
            }
        }
    }
}

/** parity: `DeOpCommands`. Puts them back in the default group rather than in no group at all. */
public class DeopCommand : Command(name = "deop", permissionLevel = PermissionLevel.ADMINS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("targets", GameProfileArgument) {
            executes { context ->
                val name = context.get<String>("targets")
                assign(context, name, context.environment.groups.defaultGroup.name)
                context.success(
                    Component.translatable("commands.deop.success", Component.text(name)),
                    broadcastToAdmins = true,
                )
                1
            }
        }
    }
}

/**
 * `group <jugador> <grupo>`: the general form the other two are shortcuts for.
 *
 * No vanilla counterpart, so no translation key. Without it the groups between `default` and
 * `operator` would be declarable in `groups.toml` and impossible to actually put anyone in.
 */
public class GroupCommand : Command(name = "group", permissionLevel = PermissionLevel.ADMINS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("player", GameProfileArgument) {
            argument("group", WordArgument) {
                executes { context ->
                    val name = context.get<String>("player")
                    val group = context.get<String>("group").lowercase()
                    if (group !in context.environment.groups.names) {
                        throw CommandSyntaxException(
                            Component.text(
                                "Unknown group '$group'. Declared groups: " +
                                    context.environment.groups.names
                                        .sorted()
                                        .joinToString(", ") + ".",
                            ),
                        )
                    }
                    assign(context, name, group)
                    context.success(
                        Component.text("Put $name in group $group."),
                        broadcastToAdmins = true,
                    )
                    1
                }
            }
        }
    }
}

/**
 * Writes the membership and applies it to the player if they are online.
 *
 * Online players resolve first: a row is only written on disconnect or on the save interval, so
 * `op` against someone who just joined used to fail with "has never joined this server". Offline
 * names resolve through the database, and a name in neither is refused rather than invented, since
 * without a profile cache a guessed UUID would never match the real player.
 */
private fun assign(
    context: CommandContext,
    playerName: String,
    group: String,
) {
    val repository =
        context.environment.playerRepository
            ?: throw CommandSyntaxException(Component.text("This server has no player database."))

    val online =
        context.environment.players
            .snapshot()
            .firstOrNull { it.name.equals(playerName, ignoreCase = true) }
    val uuid =
        online?.uuid?.toString()
            ?: repository.loadByName(playerName)?.uuid
            ?: throw CommandSyntaxException(
                Component.text("No player named '$playerName' is online or has ever joined this server."),
            )

    // A no-op when the row does not exist yet, which is exactly the first-join case above: the
    // session below carries the group instead, and it is written out with the rest of the player
    // on the next save.
    repository.setGroup(uuid, group)

    // Online players carry their group on the session, and their command tree is filtered by it, so
    // both have to be refreshed or the change would only take effect on their next login.
    online?.let { session ->
        session.group = group
        session.connection.sendCommandTree()
    }
}
