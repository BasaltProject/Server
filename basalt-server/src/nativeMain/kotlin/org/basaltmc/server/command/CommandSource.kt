package org.basaltmc.server.command

import org.basaltmc.common.logging.Logger
import org.basaltmc.server.config.GroupsConfig
import org.basaltmc.server.player.PlayerSession
import org.basaltmc.text.Component
import org.basaltmc.text.LangTable
import org.basaltmc.text.TextColor
import org.basaltmc.text.resolvePlainText

/**
 * Vanilla's own permission ladder (`Commands.LEVEL_*`), recorded per command as the parity fact it
 * is: it is what the real server demands for each of them, and what a client is eventually told
 * about itself.
 *
 * It is **not** what authorises anything here. Authorisation is by named permission, because a
 * number in a configuration file tells an operator nothing and cannot be referred to by
 * `commands.toml` or by a plugin.
 */
public object PermissionLevel {
    public const val ALL: Int = 0
    public const val MODERATORS: Int = 1
    public const val GAMEMASTERS: Int = 2
    public const val ADMINS: Int = 3
    public const val OWNERS: Int = 4
}

/**
 * Whoever ran a command: a player, or the operator at the console.
 *
 * Deliberately not a handle on "the server": commands reach the world and the player list through
 * [CommandEnvironment], which is passed in, so nothing here needs a global. That is the same reason
 * [position] lives on the source, the way vanilla's `CommandSourceStack` carries one: relative
 * coordinates (`~ ~ ~`) resolve against the runner, whoever that is.
 */
public interface CommandSource {
    public val name: String
    public val displayName: Component
    public val permissionLevel: Int

    /** Where `~` resolves from. The console stands at the world origin, as vanilla's does. */
    public val position: Triple<Double, Double, Double>

    /** The player who ran this, or `null` for the console. Commands that need one say so themselves. */
    public val player: PlayerSession?

    public fun sendSystemMessage(message: Component)

    /** Whether this source has been granted [node]. A node nobody granted is denied. */
    public fun hasPermission(node: String?): Boolean
}

/** Whether [source] may use [node]. A node with no permission of its own is open to everyone. */
public fun CommandSource.mayUse(node: CommandNode): Boolean = node.permission == null || hasPermission(node.permission)

/**
 * The console. Permission [PermissionLevel.OWNERS], like vanilla's own `MinecraftServer` source:
 * whoever can type here can already stop the process.
 */
public class ConsoleCommandSource(
    private val logger: Logger = Logger("Console"),
    private val lang: LangTable = LangTable.EMPTY,
) : CommandSource {
    // Everything, unconditionally: whoever can type here can already stop the process, and making
    // the console depend on a group would let a typo in groups.toml lock the operator out.
    override fun hasPermission(node: String?): Boolean = true

    override val name: String = "Server"
    override val displayName: Component = Component.text("Server")
    override val permissionLevel: Int = PermissionLevel.OWNERS
    override val position: Triple<Double, Double, Double> = Triple(0.0, 0.0, 0.0)
    override val player: PlayerSession? = null

    override fun sendSystemMessage(message: Component) {
        logger.info { message.resolvePlainText(lang) }
    }
}

/** A player running a command in chat. Their feedback goes back to them as a system message, as vanilla's does. */
public class PlayerCommandSource(
    override val player: PlayerSession,
    private val groups: GroupsConfig,
) : CommandSource {
    override val name: String get() = player.name
    override val displayName: Component get() = Component.text(player.name)
    override val position: Triple<Double, Double, Double> get() = Triple(player.x, player.y, player.z)

    /** Only meaningful for the vanilla-facing bridge; nothing here authorises on it. */
    override val permissionLevel: Int
        get() = if (groups.resolve(player.group, ADMIN_PERMISSION)) PermissionLevel.OWNERS else PermissionLevel.ALL

    override fun hasPermission(node: String?): Boolean = node != null && groups.resolve(player.group, node)

    override fun sendSystemMessage(message: Component) {
        player.sendSystemMessage(message)
    }
}

/**
 * Result reporting, shared by every command so the surrounding behaviour (who else sees it, what
 * reaches the log) is decided once rather than per command.
 *
 * Mirrors `CommandSourceStack.sendSuccess`/`sendFailure`: a successful command with
 * [broadcastToAdmins] set also reaches every *other* operator as `chat.type.admin`, and the console
 * log, which is vanilla's `logAdminCommands` game rule. A failure is red and goes only to whoever
 * ran it.
 */
public class CommandFeedback(
    private val environment: CommandEnvironment,
) {
    public fun success(
        source: CommandSource,
        message: Component,
        broadcastToAdmins: Boolean,
    ) {
        source.sendSystemMessage(message)
        if (!broadcastToAdmins) return
        val broadcast =
            Component
                .translatable("chat.type.admin", source.displayName, message)
                .color(TextColor.GRAY)
                .italic()
        for (player in environment.players.snapshot()) {
            if (player.uuid == source.player?.uuid) continue
            if (!environment.groups.resolve(player.group, ADMIN_PERMISSION)) continue
            player.sendSystemMessage(broadcast)
        }
        if (source.player != null) environment.logger.info { broadcast.resolvePlainText(environment.lang) }
    }

    public fun failure(
        source: CommandSource,
        message: Component,
    ) {
        source.sendSystemMessage(message.color(TextColor.RED))
    }
}

/**
 * Seeing other people's successful commands, vanilla's `chat.type.admin` broadcast.
 *
 * A permission rather than "level 2 or above", so that who receives them is decided in the same
 * file and the same way as everything else. `*` covers it, so an operator gets them by default.
 */
public const val ADMIN_PERMISSION: String = "basalt.admin.broadcast"
