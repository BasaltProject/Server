package org.basaltmc.server.command

import org.basaltmc.common.logging.Logger
import org.basaltmc.common.placeholder.PlaceholderRegistry
import org.basaltmc.server.config.CommandsConfig
import org.basaltmc.server.config.GroupsConfig
import org.basaltmc.server.player.PlayerRegistry
import org.basaltmc.server.world.LoadedWorld
import org.basaltmc.storage.PlayerRepository
import org.basaltmc.text.Component
import org.basaltmc.text.LangTable
import org.basaltmc.text.resolvePlainText

/**
 * Everything commands are allowed to reach, handed to them rather than looked up.
 *
 * This is the "no `Server.getInstance()`" rule in practice: a command gets the player list, the
 * operator list and the few server-wide facts it needs through this object, so the same dispatcher
 * runs unchanged against a test fixture. It also keeps the door open for the regional scheduler
 * later, since nothing here assumes a single global tick.
 */
public class CommandEnvironment(
    public val players: PlayerRegistry,
    public val groups: GroupsConfig,
    public val worldSeed: Long,
    /** The world a freshly imported or freshly joined player belongs to. */
    public val defaultWorldName: String = "world",
    public val maxPlayers: Int,
    public val logger: Logger = Logger("Command"),
    public val stopServer: () -> Unit = {},
    /**
     * What the console reads messages with. Clients resolve translation keys themselves against
     * their own language, so this exists only so the log shows sentences instead of keys; an empty
     * table degrades to printing the keys rather than failing.
     */
    public val lang: LangTable = LangTable.EMPTY,
    /**
     * Persisted players, as opposed to [players], which is who is online right now. Null where
     * there is no database to administer, which is how the dispatcher's own tests run.
     */
    public val playerRepository: PlayerRepository? = null,
    /** Every loaded world by name, for the commands that move players between them. */
    public val worlds: Map<String, LoadedWorld> = emptyMap(),
    /** Every placeholder this server answers, so commands can resolve text the same way files do. */
    public val placeholders: PlaceholderRegistry = PlaceholderRegistry(),
    /** `commands.toml`: which permission each command asks for, which exist, and what they answer. */
    public val commands: CommandsConfig = CommandsConfig(emptyMap(), emptyList(), emptyMap()),
) {
    /** A message every connected player sees, plus the console, the way vanilla's `broadcastSystemMessage` does. */
    public fun broadcast(message: Component) {
        for (player in players.snapshot()) player.sendSystemMessage(message)
        logger.info { message.resolvePlainText(lang) }
    }
}
