package org.basaltmc.server.runtime.command

import org.basaltmc.server.runtime.config.CommandsConfig
import org.basaltmc.server.runtime.config.GroupsConfig
import org.basaltmc.server.runtime.config.WorldDimension
import org.basaltmc.server.runtime.player.PlayerRegistry
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.world.GameRuleRegistry
import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.server.storage.AccessRepository
import org.basaltmc.server.storage.PlayerRepository
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.common.placeholder.PlaceholderRegistry
import org.basaltmc.wire.common.timings.Timings
import org.basaltmc.wire.protocol.play.ClockUpdate
import org.basaltmc.wire.protocol.play.SetTimePacket
import org.basaltmc.wire.text.Component
import org.basaltmc.wire.text.LangTable
import org.basaltmc.wire.text.resolvePlainText

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
    /**
     * Read through a supplier, not stored: `/reload` swaps the groups under a running server and a
     * captured copy would keep answering with the permissions the server started with.
     */
    private val groupsProvider: () -> GroupsConfig,
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
    /** Bans and the whitelist, or null where there is no database to hold them. */
    public val access: AccessRepository? = null,
    /** Every loaded world by name, for the commands that move players between them. */
    public val worlds: Map<String, LoadedWorld> = emptyMap(),
    /** Every placeholder this server answers, so commands can resolve text the same way files do. */
    public val placeholders: PlaceholderRegistry = PlaceholderRegistry(),
    /** Where a command's own cost is recorded, for `/timings`. Off unless it has been switched on. */
    public val timings: Timings = Timings(),
    /**
     * What a `#tag` in a block predicate resolves to, or null for a tag nobody declared.
     *
     * A function rather than the tag table itself: the table is loaded from the operator's own
     * server jar and is absent when that download has not happened, and a command should then fail
     * on the tag by name rather than on a missing service.
     */
    public val blockTagMembers: (Identifier) -> Set<Identifier>? = { null },
    /** The same for item tags, which `/clear` and the item predicates use. */
    public val itemTagMembers: (Identifier) -> Set<Identifier>? = { null },
    /** `commands.toml`: which permission each command asks for, which exist, and what they answer. */
    private val commandsProvider: () -> CommandsConfig = { CommandsConfig(emptyMap(), emptyList(), emptyMap()) },
) {
    /** A message every connected player sees, plus the console, the way vanilla's `broadcastSystemMessage` does. */
    public val groups: GroupsConfig get() = groupsProvider()

    public val commands: CommandsConfig get() = commandsProvider()

    public fun broadcast(message: Component) {
        for (player in players.snapshot()) player.sendSystemMessage(message)
        logger.info { message.resolvePlainText(lang) }
    }

    /** Everyone standing in [world], for the packets that only mean anything to them. */
    public fun playersIn(world: LoadedWorld): List<PlayerSession> =
        players.snapshot().filter { it.worldName == world.name }

    /** Pushes [world]'s clock to whoever is in it, after a `/time` moved it. */
    public fun syncTime(world: LoadedWorld) {
        val packet = world.timePacket()
        for (player in playersIn(world)) {
            player.connection.sendPlayPacket(SetTimePacket.ID) { packet.write(it) }
        }
    }

    /** Re-sends every chunk in a block range to whoever is in [world]: for changes too big to patch. */
    public fun syncChunks(
        world: LoadedWorld,
        minX: Int,
        minZ: Int,
        maxX: Int,
        maxZ: Int,
    ) {
        val players = playersIn(world)
        if (players.isEmpty()) return
        for (chunkX in (minX shr 4)..(maxX shr 4)) {
            for (chunkZ in (minZ shr 4)..(maxZ shr 4)) {
                for (player in players) player.connection.resendChunk(chunkX, chunkZ)
            }
        }
    }

    /** Pushes [world]'s weather, which is three game events rather than one packet, as vanilla sends it. */
    public fun syncWeather(world: LoadedWorld) {
        for (player in playersIn(world)) player.connection.sendWeather(world.weather)
    }
}

/** The clock update [world] would send right now, for a join or a change. */
public fun LoadedWorld.timePacket(): SetTimePacket {
    val clockId =
        if (declaration.dimension == WorldDimension.END) {
            SetTimePacket.END_CLOCK
        } else {
            SetTimePacket.OVERWORLD_CLOCK
        }
    return SetTimePacket(
        gameTime = gameTime,
        clocks =
            listOf(
                ClockUpdate(
                    clockId = clockId,
                    totalTicks = clock.totalTicks,
                    partialTick = 0f,
                    rate = clock.networkRate(gameRules[GameRuleRegistry.ADVANCE_TIME]),
                ),
            ),
    )
}
