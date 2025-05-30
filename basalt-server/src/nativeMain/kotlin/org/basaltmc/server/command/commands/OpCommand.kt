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
 * Writes the membership and applies it to the player if they happen to be online.
 *
 * Only players the server has seen before can be assigned: without a profile cache there is no way
 * to turn a name it has never met into a UUID, and inventing one would silently create a row that
 * the real player would never match. Vanilla resolves that through Mojang's API; this says so
 * instead of pretending.
 */
private fun assign(
    context: CommandContext,
    playerName: String,
    group: String,
) {
    val repository =
        context.environment.playerRepository
            ?: throw CommandSyntaxException(Component.text("This server has no player database."))
    val stored =
        repository.loadByName(playerName)
            ?: throw CommandSyntaxException(
                Component.text("No player named '$playerName' has ever joined this server."),
            )
    repository.setGroup(stored.uuid, group)

    // Online players carry their group on the session, and their command tree is filtered by it, so
    // both have to be refreshed or the change would only take effect on their next login.
    context.environment.players
        .snapshot()
        .firstOrNull { it.uuid.toString() == stored.uuid }
        ?.let { session ->
            session.group = group
            session.connection.sendCommandTree()
        }
}
