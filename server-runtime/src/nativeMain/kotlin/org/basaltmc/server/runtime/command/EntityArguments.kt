package org.basaltmc.server.runtime.command

import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.wire.protocol.play.ArgumentParser
import org.basaltmc.wire.text.Component

/**
 * One thing a command acts on: a player, or a summoned entity.
 *
 * Commands that only make sense for a player ask for [PlayersArgument]; the ones vanilla lets you
 * point at anything (`/kill`, `/damage`, `/tag`, `/rotate`, `/ride`) take this instead.
 */
public sealed interface CommandTarget {
    public val name: String
    public val entityId: Int
    public val x: Double
    public val y: Double
    public val z: Double

    public class Player(
        public val session: PlayerSession,
    ) : CommandTarget {
        override val name: String get() = session.name
        override val entityId: Int get() = session.entityId
        override val x: Double get() = session.x
        override val y: Double get() = session.y
        override val z: Double get() = session.z
    }

    public class Entity(
        public val entity: ServerEntity,
    ) : CommandTarget {
        override val name: String get() = entity.customName ?: entity.type.path
        override val entityId: Int get() = entity.entityId
        override val x: Double get() = entity.x
        override val y: Double get() = entity.y
        override val z: Double get() = entity.z
    }
}

/**
 * `minecraft:entity`, over players *and* summoned entities.
 *
 * `@e` means everything, `@a` only players, and a bare name matches a player or an entity's custom
 * name, which is how vanilla resolves one too. Selector filters are still a documented gap.
 */
public class EntitiesArgument(
    private val single: Boolean = false,
) : ArgumentType<List<CommandTarget>> {
    override val wire: ArgumentParser = ArgumentParser.Entity(single = single, playersOnly = false)

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): List<CommandTarget> {
        val start = reader.cursor
        val players = environment.players.snapshot()
        val entities =
            source
                .worldIn(environment)
                ?.state
                ?.entities()
                ?.filter { !it.removed }
                .orEmpty()

        val matched: List<CommandTarget> =
            if (reader.canRead() && reader.peek() == '@') {
                reader.skip()
                val selector = if (reader.canRead()) reader.read() else ' '
                if (reader.canRead() && reader.peek() == '[') {
                    reader.cursor = start
                    throw reader.syntaxError("Selector filters are not supported yet")
                }
                when (selector) {
                    's' -> listOfNotNull(source.player).map { CommandTarget.Player(it) }
                    'p' -> nearestPlayer(source, players)?.let { listOf(CommandTarget.Player(it)) }.orEmpty()
                    'r' -> players.randomOrNull()?.let { listOf(CommandTarget.Player(it)) }.orEmpty()
                    'a' -> players.map { CommandTarget.Player(it) }
                    'n' -> nearest(source, players, entities)?.let(::listOf).orEmpty()
                    'e' -> players.map { CommandTarget.Player(it) } + entities.map { CommandTarget.Entity(it) }
                    else -> {
                        reader.cursor = start
                        throw reader.syntaxError("Unknown selector type '@$selector'")
                    }
                }
            } else {
                val name = reader.readString()
                players.filter { it.name.equals(name, ignoreCase = true) }.map { CommandTarget.Player(it) } +
                    entities
                        .filter { it.customName.equals(name, ignoreCase = true) || it.type.path == name }
                        .map { CommandTarget.Entity(it) }
            }

        if (matched.isEmpty()) {
            reader.cursor = start
            throw CommandSyntaxException(
                Component.translatable("argument.entity.notfound.entity"),
                reader.string,
                reader.cursor,
            )
        }
        if (single && matched.size > 1) {
            reader.cursor = start
            throw CommandSyntaxException(
                Component.translatable("argument.entity.toomany"),
                reader.string,
                reader.cursor,
            )
        }
        return matched
    }

    private fun nearestPlayer(
        source: CommandSource,
        players: List<PlayerSession>,
    ): PlayerSession? {
        val (x, y, z) = source.position
        return players.minByOrNull { squaredDistance(it.x - x, it.y - y, it.z - z) }
    }

    private fun nearest(
        source: CommandSource,
        players: List<PlayerSession>,
        entities: List<ServerEntity>,
    ): CommandTarget? {
        val (x, y, z) = source.position
        val all = players.map { CommandTarget.Player(it) } + entities.map { CommandTarget.Entity(it) }
        return all.minByOrNull { squaredDistance(it.x - x, it.y - y, it.z - z) }
    }

    private fun squaredDistance(
        dx: Double,
        dy: Double,
        dz: Double,
    ): Double = dx * dx + dy * dy + dz * dz
}
