package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.GameProfileArgument
import org.basaltmc.server.runtime.command.MessageArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.WordArgument
import org.basaltmc.server.storage.BanKind
import org.basaltmc.server.storage.BanRecord
import org.basaltmc.server.storage.WhitelistEntry
import org.basaltmc.wire.auth.OfflineUuid
import org.basaltmc.wire.text.Component
import kotlin.time.Clock
import kotlin.time.ExperimentalTime

/** parity: `BanPlayerCommands`. A banned player is disconnected on the spot, as vanilla does. */
@OptIn(ExperimentalTime::class)
public class BanCommand : Command(name = "ban", permissionLevel = PermissionLevel.ADMINS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("targets", GameProfileArgument) {
            executes { context -> ban(context, null) }
            argument("reason", MessageArgument) { executes { context -> ban(context, context.get("reason")) } }
        }
    }

    private fun ban(
        context: CommandContext,
        reason: String?,
    ): Int {
        val access =
            context.environment.access
                ?: throw CommandSyntaxException(Component.text("This server has no player database to ban into."))
        val name = context.get<String>("targets")
        val online =
            context.environment.players
                .snapshot()
                .firstOrNull { it.name.equals(name, ignoreCase = true) }
        val uuid = online?.uuid?.toString() ?: storedUuid(context, name)
        if (access.banOf(uuid, BanKind.PLAYER) != null) {
            throw CommandSyntaxException(Component.translatable("commands.ban.failed"))
        }
        val text = reason ?: DEFAULT_REASON
        access.ban(
            BanRecord(
                subject = uuid,
                kind = BanKind.PLAYER,
                name = online?.name ?: name,
                reason = text,
                source = context.source.name,
                createdAtEpochSeconds = Clock.System.now().epochSeconds,
                expiresAtEpochSeconds = null,
            ),
        )
        online?.connection?.disconnect(
            Component.translatable("multiplayer.disconnect.banned.reason", Component.text(text)),
        )
        context.success(
            Component.translatable("commands.ban.success", Component.text(name), Component.text(text)),
            broadcastToAdmins = true,
        )
        return 1
    }

    /** A name nobody has played under still bans: the UUID is the offline one, as vanilla resolves it. */
    private fun storedUuid(
        context: CommandContext,
        name: String,
    ): String =
        context.environment.playerRepository
            ?.loadByName(name)
            ?.uuid
            ?: OfflineUuid.forUsername(name).toString()

    internal companion object {
        const val DEFAULT_REASON = "Banned by an operator."
    }
}

/** parity: `BanIpCommands`, including its "this is a player, ban their address" form. */
@OptIn(ExperimentalTime::class)
public class BanIpCommand : Command(name = "ban-ip", permissionLevel = PermissionLevel.ADMINS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("target", WordArgument) {
            executes { context -> ban(context, null) }
            argument("reason", MessageArgument) { executes { context -> ban(context, context.get("reason")) } }
        }
    }

    private fun ban(
        context: CommandContext,
        reason: String?,
    ): Int {
        val access =
            context.environment.access
                ?: throw CommandSyntaxException(Component.text("This server has no player database to ban into."))
        val target = context.get<String>("target")
        val online = context.environment.players.snapshot()
        val address =
            if (looksLikeAddress(target)) {
                target
            } else {
                online.firstOrNull { it.name.equals(target, ignoreCase = true) }?.address
                    ?: throw CommandSyntaxException(Component.translatable("commands.banip.invalid"))
            }
        if (access.banOf(address, BanKind.IP) != null) {
            throw CommandSyntaxException(Component.translatable("commands.banip.failed"))
        }
        val text = reason ?: BanCommand.DEFAULT_REASON
        access.ban(
            BanRecord(
                subject = address,
                kind = BanKind.IP,
                name = null,
                reason = text,
                source = context.source.name,
                createdAtEpochSeconds = Clock.System.now().epochSeconds,
                expiresAtEpochSeconds = null,
            ),
        )
        val affected = online.filter { it.address == address }
        for (player in affected) {
            player.connection.disconnect(
                Component.translatable("multiplayer.disconnect.banned_ip.reason", Component.text(text)),
            )
        }
        context.success(
            Component.translatable(
                "commands.banip.success",
                Component.text(address),
                Component.text(text),
            ),
            broadcastToAdmins = true,
        )
        return affected.size + 1
    }

    private fun looksLikeAddress(text: String): Boolean =
        text.count { it == '.' } == 3 && text.split('.').all { part -> part.toIntOrNull() in 0..255 }
}

/** parity: `PardonCommand` and `PardonIpCommand`, which refuse when there was nothing to lift. */
public class PardonCommand(
    private val ip: Boolean,
) : Command(name = if (ip) "pardon-ip" else "pardon", permissionLevel = PermissionLevel.ADMINS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument(if (ip) "target" else "targets", if (ip) WordArgument else GameProfileArgument) {
            // parity: `PardonCommand` completes with who is actually banned, not with who is online.
            suggestsFromServer { context ->
                context.matching(
                    context.environment.access
                        ?.bans(if (ip) BanKind.IP else BanKind.PLAYER)
                        .orEmpty()
                        .mapNotNull { if (ip) it.subject else it.name },
                )
            }
            executes { context -> pardon(context) }
        }
    }

    private fun pardon(context: CommandContext): Int {
        val access =
            context.environment.access
                ?: throw CommandSyntaxException(Component.text("This server has no player database to pardon from."))
        val target = context.get<String>(if (ip) "target" else "targets")
        val subject =
            if (ip) {
                target
            } else {
                context.environment.playerRepository
                    ?.loadByName(target)
                    ?.uuid
                    ?: OfflineUuid.forUsername(target).toString()
            }
        val kind = if (ip) BanKind.IP else BanKind.PLAYER
        if (!access.pardon(subject, kind)) {
            throw CommandSyntaxException(
                Component.translatable(if (ip) "commands.pardonip.failed" else "commands.pardon.failed"),
            )
        }
        context.success(
            Component.translatable(
                if (ip) "commands.pardonip.success" else "commands.pardon.success",
                Component.text(target),
            ),
            broadcastToAdmins = true,
        )
        return 1
    }
}

/** parity: `BanListCommands`. */
public class BanListCommand : Command(name = "banlist", permissionLevel = PermissionLevel.ADMINS) {
    override fun build(root: CommandNodeBuilder) {
        root.executes { context -> list(context, null) }
        root.literal("players") { executes { context -> list(context, BanKind.PLAYER) } }
        root.literal("ips") { executes { context -> list(context, BanKind.IP) } }
    }

    private fun list(
        context: CommandContext,
        kind: BanKind?,
    ): Int {
        val access =
            context.environment.access
                ?: throw CommandSyntaxException(Component.text("This server has no player database to read bans from."))
        val bans =
            when (kind) {
                null -> access.bans(BanKind.PLAYER) + access.bans(BanKind.IP)
                else -> access.bans(kind)
            }
        if (bans.isEmpty()) {
            context.success(Component.translatable("commands.banlist.none"))
            return 0
        }
        context.success(
            Component.translatable("commands.banlist.list", Component.text(bans.size.toString())),
        )
        for (ban in bans) {
            context.success(
                Component.translatable(
                    "commands.banlist.entry",
                    Component.text(ban.name ?: ban.subject),
                    Component.text(ban.source),
                    Component.text(ban.reason),
                ),
            )
        }
        return bans.size
    }
}

/**
 * parity: `WhitelistCommand`.
 *
 * Whether the whitelist is enforced is `server.toml`'s `whitelist`, so `on` and `off` report that
 * the file decides rather than silently disagreeing with it: an operator who turns it on here and
 * restarts would otherwise find it off again.
 */
@OptIn(ExperimentalTime::class)
public class WhitelistCommand(
    private val enforced: () -> Boolean,
    private val setEnforced: (Boolean) -> Unit,
) : Command(name = "whitelist", permissionLevel = PermissionLevel.ADMINS) {
    override fun build(root: CommandNodeBuilder) {
        root.literal("on") { executes { context -> toggle(context, true) } }
        root.literal("off") { executes { context -> toggle(context, false) } }
        root.literal("list") { executes { context -> list(context) } }
        root.literal("add") {
            argument("targets", GameProfileArgument) {
                suggestsFromServer { context ->
                    val listed =
                        context.environment.access
                            ?.whitelist()
                            .orEmpty()
                            .map { it.name }
                            .toSet()
                    context.matching(
                        context.environment.players
                            .snapshot()
                            .map { it.name } - listed,
                    )
                }
                executes { context -> add(context) }
            }
        }
        root.literal("remove") {
            argument("targets", GameProfileArgument) {
                suggestsFromServer { context ->
                    context.matching(
                        context.environment.access
                            ?.whitelist()
                            .orEmpty()
                            .map { it.name },
                    )
                }
                executes { context -> remove(context) }
            }
        }
        root.literal("reload") {
            executes { context ->
                context.success(Component.translatable("commands.whitelist.reloaded"))
                1
            }
        }
    }

    private fun access(context: CommandContext) =
        context.environment.access
            ?: throw CommandSyntaxException(Component.text("This server has no player database to whitelist with."))

    private fun toggle(
        context: CommandContext,
        on: Boolean,
    ): Int {
        if (enforced() == on) {
            throw CommandSyntaxException(
                Component.translatable(if (on) "commands.whitelist.alreadyOn" else "commands.whitelist.alreadyOff"),
            )
        }
        setEnforced(on)
        context.success(
            Component.translatable(if (on) "commands.whitelist.enabled" else "commands.whitelist.disabled"),
            broadcastToAdmins = true,
        )
        return 1
    }

    private fun list(context: CommandContext): Int {
        val entries = access(context).whitelist()
        context.success(
            if (entries.isEmpty()) {
                Component.translatable("commands.whitelist.none")
            } else {
                Component.translatable(
                    "commands.whitelist.list",
                    Component.text(entries.size.toString()),
                    Component.text(entries.joinToString(", ") { it.name }),
                )
            },
        )
        return entries.size
    }

    private fun add(context: CommandContext): Int {
        val access = access(context)
        val name = context.get<String>("targets")
        if (access.whitelistedByName(name) != null) {
            throw CommandSyntaxException(Component.translatable("commands.whitelist.add.failed"))
        }
        val uuid =
            context.environment.players
                .snapshot()
                .firstOrNull { it.name.equals(name, ignoreCase = true) }
                ?.uuid
                ?.toString()
                ?: context.environment.playerRepository
                    ?.loadByName(name)
                    ?.uuid
                ?: OfflineUuid.forUsername(name).toString()
        access.addToWhitelist(WhitelistEntry(uuid, name, Clock.System.now().epochSeconds))
        context.success(
            Component.translatable("commands.whitelist.add.success", Component.text(name)),
            broadcastToAdmins = true,
        )
        return 1
    }

    private fun remove(context: CommandContext): Int {
        val access = access(context)
        val name = context.get<String>("targets")
        val entry =
            access.whitelistedByName(name)
                ?: throw CommandSyntaxException(Component.translatable("commands.whitelist.remove.failed"))
        access.removeFromWhitelist(entry.uuid)
        context.success(
            Component.translatable("commands.whitelist.remove.success", Component.text(name)),
            broadcastToAdmins = true,
        )
        return 1
    }
}
