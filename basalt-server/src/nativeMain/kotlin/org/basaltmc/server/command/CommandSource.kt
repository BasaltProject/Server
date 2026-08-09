package org.basaltmc.server.command

import org.basaltmc.common.logging.Logger
import org.basaltmc.server.player.PlayerSession
import org.basaltmc.text.Component
import org.basaltmc.text.LangTable
import org.basaltmc.text.TextColor
import org.basaltmc.text.resolvePlainText

/** Vanilla's own permission ladder (`Commands.LEVEL_*`), used verbatim by every command's requirement. */
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

    public fun hasPermission(level: Int): Boolean = permissionLevel >= level
}

/**
 * The console. Permission [PermissionLevel.OWNERS], like vanilla's own `MinecraftServer` source:
 * whoever can type here can already stop the process.
 */
public class ConsoleCommandSource(
    private val logger: Logger = Logger("Console"),
    private val lang: LangTable = LangTable.EMPTY,
) : CommandSource {
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
    override val permissionLevel: Int,
) : CommandSource {
    override val name: String get() = player.name
    override val displayName: Component get() = Component.text(player.name)
    override val position: Triple<Double, Double, Double> get() = Triple(player.x, player.y, player.z)

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
            if (environment.permissionLevelOf(player.name) < PermissionLevel.MODERATORS) continue
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
