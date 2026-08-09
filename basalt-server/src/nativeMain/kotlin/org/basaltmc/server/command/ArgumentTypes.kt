package org.basaltmc.server.command

import org.basaltmc.protocol.play.ArgumentParser
import org.basaltmc.protocol.play.Gamemode
import org.basaltmc.server.player.PlayerSession
import org.basaltmc.text.Component

/** A single word, `brigadier:string` in its `SINGLE_WORD` form. */
public object WordArgument : ArgumentType<String> {
    override val wire: ArgumentParser = ArgumentParser.BrigadierString(ArgumentParser.BrigadierString.Kind.SINGLE_WORD)

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): String = reader.readString()
}

/** Everything left on the line, `brigadier:string` in its `GREEDY_PHRASE` form. */
public object GreedyStringArgument : ArgumentType<String> {
    override val wire: ArgumentParser =
        ArgumentParser.BrigadierString(
            ArgumentParser.BrigadierString.Kind.GREEDY_PHRASE,
        )

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): String {
        val text = reader.remaining
        reader.cursor = reader.string.length
        return text
    }
}

/**
 * `minecraft:message`: the rest of the line, as chat text. Parsed exactly like a greedy string
 * here; what makes it a distinct type is that vanilla resolves `@selector` mentions and signs it,
 * neither of which exists yet (documented gap, same as signed chat).
 */
public object MessageArgument : ArgumentType<String> {
    override val wire: ArgumentParser = ArgumentParser.Message

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): String {
        val text = reader.remaining
        reader.cursor = reader.string.length
        return text
    }
}

/**
 * `minecraft:game_profile`: a player name, which vanilla resolves against its profile cache so it
 * works on someone who is offline. Basalt has no profile cache yet, so this only carries the name
 * through; `/op` on a name nobody has ever used is accepted, exactly as vanilla accepts it.
 */
public object GameProfileArgument : ArgumentType<String> {
    override val wire: ArgumentParser = ArgumentParser.GameProfile

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): String = reader.readString()
}

public class IntegerArgument(
    private val min: Int = Int.MIN_VALUE,
    private val max: Int = Int.MAX_VALUE,
) : ArgumentType<Int> {
    override val wire: ArgumentParser = ArgumentParser.BrigadierInteger(min, max)

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): Int {
        val start = reader.cursor
        val value = reader.readInt()
        if (value < min) {
            reader.cursor = start
            throw reader.syntaxError("Integer must not be less than $min, found $value")
        }
        if (value > max) {
            reader.cursor = start
            throw reader.syntaxError("Integer must not be more than $max, found $value")
        }
        return value
    }
}

/** `minecraft:gamemode`. Accepts the four names vanilla's `GameType` serializes to, nothing else. */
public object GameModeArgument : ArgumentType<Gamemode> {
    override val wire: ArgumentParser = ArgumentParser.GameMode

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): Gamemode {
        val start = reader.cursor
        val name = reader.readUnquotedString()
        return Gamemode.entries.firstOrNull { it.name.equals(name, ignoreCase = true) }
            ?: run {
                reader.cursor = start
                throw CommandSyntaxException(
                    Component.translatable("argument.gamemode.invalid", Component.text(name)),
                    reader.string,
                    reader.cursor,
                )
            }
    }
}

/**
 * `minecraft:entity` restricted to players, which is every selector this server can answer: there
 * are no other entities yet.
 *
 * Understands `@s`, `@p`, `@a`, `@r` and a plain player name. Selector *filters* (`@a[distance=..5]`
 * and the rest of the bracket syntax) are a documented gap: they parse as an error rather than
 * being silently ignored, so nobody gets a wrong set of targets back.
 */
public class PlayersArgument(
    private val single: Boolean,
) : ArgumentType<List<PlayerSession>> {
    override val wire: ArgumentParser = ArgumentParser.Entity(single = single, playersOnly = true)

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): List<PlayerSession> {
        val start = reader.cursor
        val online = environment.players.snapshot()
        val matched =
            if (reader.canRead() && reader.peek() == '@') {
                reader.skip()
                val selector = if (reader.canRead()) reader.read() else ' '
                if (reader.canRead() && reader.peek() == '[') {
                    reader.cursor = start
                    throw reader.syntaxError("Selector filters are not supported yet")
                }
                when (selector) {
                    's' -> listOfNotNull(source.player)
                    'p' -> nearestTo(source, online).let(::listOfNotNull)
                    'r' -> online.randomOrNull()?.let(::listOf).orEmpty()
                    'a', 'e' -> online
                    else -> {
                        reader.cursor = start
                        throw reader.syntaxError("Unknown selector type '@$selector'")
                    }
                }
            } else {
                val name = reader.readString()
                online.filter { it.name.equals(name, ignoreCase = true) }
            }

        if (matched.isEmpty()) {
            reader.cursor = start
            throw CommandSyntaxException(
                Component.translatable("argument.entity.notfound.player"),
                reader.string,
                reader.cursor,
            )
        }
        if (single && matched.size > 1) {
            reader.cursor = start
            throw CommandSyntaxException(
                Component.translatable("argument.player.toomany"),
                reader.string,
                reader.cursor,
            )
        }
        return matched
    }

    private fun nearestTo(
        source: CommandSource,
        online: List<PlayerSession>,
    ): PlayerSession? {
        val (x, y, z) = source.position
        return online.minByOrNull { player ->
            val dx = player.x - x
            val dy = player.y - y
            val dz = player.z - z
            dx * dx + dy * dy + dz * dz
        }
    }
}

/**
 * One coordinate of a position: absolute, or relative to whoever ran the command (`~`).
 *
 * The +0.5 on a bare integer is vanilla's own `WorldCoordinate.parseDouble` centre correction: `/tp
 * 10 64 10` puts you in the middle of that block, not on its corner, and only when the number has
 * no decimal point and no `~`. Local (`^`) coordinates are a documented gap and are rejected
 * outright rather than being read as absolute.
 */
public data class WorldCoordinate(
    val relative: Boolean,
    val value: Double,
) {
    public fun resolve(base: Double): Double = if (relative) base + value else value

    public companion object {
        public fun parse(
            reader: StringReader,
            centerCorrect: Boolean,
        ): WorldCoordinate {
            if (reader.canRead() && reader.peek() == '^') {
                throw CommandSyntaxException(Component.translatable("argument.pos.mixed"), reader.string, reader.cursor)
            }
            if (!reader.canRead()) {
                throw CommandSyntaxException(
                    Component.translatable("argument.pos.missing.double"),
                    reader.string,
                    reader.cursor,
                )
            }
            val relative = reader.peek() == '~'
            if (relative) reader.skip()
            val start = reader.cursor
            val value = if (reader.canRead() && reader.peek() != ' ') reader.readDouble() else 0.0
            val text = reader.string.substring(start, reader.cursor)
            if (relative && text.isEmpty()) return WorldCoordinate(true, 0.0)
            val corrected = if (!text.contains('.') && !relative && centerCorrect) value + 0.5 else value
            return WorldCoordinate(relative, corrected)
        }
    }
}

/** A parsed `x y z`, still unresolved: `~` needs whoever runs the command to mean anything. */
public data class Coordinates(
    val x: WorldCoordinate,
    val y: WorldCoordinate,
    val z: WorldCoordinate,
) {
    public fun resolve(base: Triple<Double, Double, Double>): Triple<Double, Double, Double> =
        Triple(x.resolve(base.first), y.resolve(base.second), z.resolve(base.third))
}

/** `minecraft:vec3`. Centre-corrects bare integers, as vanilla's own `Vec3Argument.vec3()` does. */
public object Vec3Argument : ArgumentType<Coordinates> {
    override val wire: ArgumentParser = ArgumentParser.Vec3

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): Coordinates {
        val start = reader.cursor
        val x = WorldCoordinate.parse(reader, centerCorrect = true)
        if (!reader.canRead() || reader.peek() != ' ') {
            reader.cursor = start
            throw CommandSyntaxException(Component.translatable("argument.pos3d.incomplete"), reader.string, start)
        }
        reader.skip()
        val y = WorldCoordinate.parse(reader, centerCorrect = false)
        if (!reader.canRead() || reader.peek() != ' ') {
            reader.cursor = start
            throw CommandSyntaxException(Component.translatable("argument.pos3d.incomplete"), reader.string, start)
        }
        reader.skip()
        val z = WorldCoordinate.parse(reader, centerCorrect = true)
        return Coordinates(x, y, z)
    }
}
