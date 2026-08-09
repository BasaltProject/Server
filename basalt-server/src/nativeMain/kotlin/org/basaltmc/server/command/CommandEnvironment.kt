package org.basaltmc.server.command

import org.basaltmc.common.logging.Logger
import org.basaltmc.io.NativeMutex
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
    public val operators: OperatorList,
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
) {
    public fun permissionLevelOf(playerName: String): Int = operators.levelOf(playerName)

    /** A message every connected player sees, plus the console, the way vanilla's `broadcastSystemMessage` does. */
    public fun broadcast(message: Component) {
        for (player in players.snapshot()) player.sendSystemMessage(message)
        logger.info { message.resolvePlainText(lang) }
    }
}

/**
 * Who is an operator, and at what level.
 *
 * Vanilla keeps this in `ops.json`; Basalt keeps it in `basalt.toml`'s `[server] operators`,
 * because this project has exactly one configuration file (a deliberate product decision, not an
 * oversight). [onChanged] is how `/op` and `/deop` persist a change back to that file: the list
 * itself only knows the names.
 */
public class OperatorList(
    names: Collection<String>,
    private val level: Int,
    private val onChanged: (Set<String>) -> Unit = {},
) {
    private val mutex = NativeMutex()
    private val operators = names.mapTo(mutableSetOf()) { it.lowercase() }

    public fun levelOf(playerName: String): Int =
        mutex.withLock { if (playerName.lowercase() in operators) level else PermissionLevel.ALL }

    public fun names(): List<String> = mutex.withLock { operators.toList().sorted() }

    /** Returns false if they were already an operator, so `/op` can report vanilla's "Nothing changed". */
    public fun add(playerName: String): Boolean {
        val changed = mutex.withLock { operators.add(playerName.lowercase()) }
        if (changed) onChanged(mutex.withLock { operators.toSet() })
        return changed
    }

    public fun remove(playerName: String): Boolean {
        val changed = mutex.withLock { operators.remove(playerName.lowercase()) }
        if (changed) onChanged(mutex.withLock { operators.toSet() })
        return changed
    }
}
