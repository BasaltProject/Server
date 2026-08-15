package org.basaltmc.server.runtime.command

import org.basaltmc.server.registry.EntityTypes
import org.basaltmc.server.runtime.entity.ServerEntity
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.protocol.play.ArgumentParser
import org.basaltmc.wire.text.Component

/**
 * One thing a command acts on: a player, or a summoned entity.
 *
 * Commands that only make sense for a player ask for [PlayersArgument]; the ones vanilla lets you
 * point at anything (`/kill`, `/damage`, `/tag`, `/rotate`, `/ride`) take this instead. Everything
 * a selector filter can ask about is on the interface, so the filters do not have to know which of
 * the two they are looking at.
 */
public sealed interface CommandTarget {
    public val name: String
    public val entityId: Int
    public val uuid: String
    public val type: Identifier
    public val worldName: String
    public val x: Double
    public val y: Double
    public val z: Double
    public val yaw: Float
    public val pitch: Float
    public val tags: Set<String>
    public val alive: Boolean

    /** How the scoreboard knows it. parity: `Entity.getScoreboardName`, a player's name and anything else's UUID. */
    public val scoreboardName: String

    /** Its collision box, for `dx/dy/dz`. Zero where this build has no size for it. */
    public val width: Double
    public val height: Double

    public class Player(
        public val session: PlayerSession,
    ) : CommandTarget {
        override val name: String get() = session.name
        override val entityId: Int get() = session.entityId
        override val uuid: String get() = session.uuid.toString()
        override val type: Identifier get() = EntityTypes.PLAYER
        override val worldName: String get() = session.worldName
        override val x: Double get() = session.x
        override val y: Double get() = session.y
        override val z: Double get() = session.z
        override val yaw: Float get() = session.yaw
        override val pitch: Float get() = session.pitch
        override val tags: Set<String> get() = session.tags
        override val alive: Boolean get() = !session.dead
        override val scoreboardName: String get() = session.name
        override val width: Double get() = PLAYER_WIDTH
        override val height: Double get() = PLAYER_HEIGHT
    }

    public class Entity(
        public val entity: ServerEntity,
        override val worldName: String = "",
    ) : CommandTarget {
        override val name: String get() = entity.customName ?: entity.type.path
        override val entityId: Int get() = entity.entityId
        override val uuid: String get() = entity.uuid.toString()
        override val type: Identifier get() = entity.type
        override val x: Double get() = entity.x
        override val y: Double get() = entity.y
        override val z: Double get() = entity.z
        override val yaw: Float get() = entity.yaw
        override val pitch: Float get() = entity.pitch
        override val tags: Set<String> get() = entity.tags
        override val alive: Boolean get() = !entity.removed && entity.health > 0.0
        override val scoreboardName: String get() = entity.uuid.toString()

        /** A point: entity dimensions are a table this build does not have, so `dx=` sees no volume. */
        override val width: Double get() = 0.0
        override val height: Double get() = 0.0
    }

    public companion object {
        /** parity: `EntityTypes.PLAYER`'s dimensions, which is what a `dx=` box is tested against. */
        public const val PLAYER_WIDTH: Double = 0.6
        public const val PLAYER_HEIGHT: Double = 1.8
    }
}

/**
 * `minecraft:entity`, over players *and* summoned entities.
 *
 * The whole selector grammar lives in [EntitySelector]; this is the argument type around it, so
 * `single` is checked here and the search itself is shared with [PlayersArgument].
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
        val matched =
            EntitySelector
                .parse(reader, source, environment)
                .select(source, environment, playersOnly = false)
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
}
