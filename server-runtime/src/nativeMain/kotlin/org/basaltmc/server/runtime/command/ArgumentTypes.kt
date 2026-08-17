package org.basaltmc.server.runtime.command

import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.wire.protocol.play.ArgumentParser
import org.basaltmc.wire.protocol.play.Gamemode
import org.basaltmc.wire.text.Component

/** A single word, `brigadier:string` in its `SINGLE_WORD` form. */
public object WordArgument : ArgumentType<String> {
    override val wire: ArgumentParser = ArgumentParser.BrigadierString(ArgumentParser.BrigadierString.Kind.SINGLE_WORD)

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): String = reader.readString()
}

/** `brigadier:bool`, which accepts exactly `true` and `false`. */
public object BoolArgument : ArgumentType<Boolean> {
    override val wire: ArgumentParser = ArgumentParser.BrigadierBool

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): Boolean {
        val start = reader.cursor
        val word = reader.readUnquotedString()
        return word.toBooleanStrictOrNull() ?: run {
            reader.cursor = start
            throw reader.syntaxError(
                if (word.isEmpty()) "Expected bool" else "Invalid boolean, expected true or false but found '$word'",
            )
        }
    }
}

/** `brigadier:double`, bounded the way the command that declares it needs. */
public class DoubleArgument(
    private val min: Double = -Double.MAX_VALUE,
    private val max: Double = Double.MAX_VALUE,
) : ArgumentType<Double> {
    override val wire: ArgumentParser = ArgumentParser.BrigadierDouble(min, max)

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): Double {
        val start = reader.cursor
        val value = reader.readDouble()
        if (value < min) {
            reader.cursor = start
            throw reader.syntaxError("Double must not be less than $min, found $value")
        }
        if (value > max) {
            reader.cursor = start
            throw reader.syntaxError("Double must not be more than $max, found $value")
        }
        return value
    }
}

/** `brigadier:float`, bounded the way the command that declares it needs. */
public class FloatArgument(
    private val min: Float = -Float.MAX_VALUE,
    private val max: Float = Float.MAX_VALUE,
) : ArgumentType<Float> {
    override val wire: ArgumentParser = ArgumentParser.BrigadierFloat(min, max)

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): Float {
        val start = reader.cursor
        val value = reader.readDouble().toFloat()
        if (value < min) {
            reader.cursor = start
            throw reader.syntaxError("Float must not be less than $min, found $value")
        }
        if (value > max) {
            reader.cursor = start
            throw reader.syntaxError("Float must not be more than $max, found $value")
        }
        return value
    }
}

/** `minecraft:angle`: one yaw in degrees, absolute or relative to whoever is being pointed. */
public object AngleArgument : ArgumentType<Float> {
    override val wire: ArgumentParser = ArgumentParser.Angle

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): Float {
        if (!reader.canRead()) throw reader.syntaxError("Expected angle")
        val relative = reader.peek() == '~'
        if (relative) reader.skip()
        val value = if (reader.canRead() && reader.peek() != ' ') reader.readDouble().toFloat() else 0f
        // parity: `AngleArgument` wraps into -180..180 rather than rejecting 540.
        return value.mod(360f).let { if (it >= 180f) it - 360f else it }
    }
}

/** `minecraft:rotation`: a `yaw pitch` pair, each absolute or relative. */
public object RotationArgument : ArgumentType<Pair<Float, Float>> {
    override val wire: ArgumentParser = ArgumentParser.Rotation

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): Pair<Float, Float> {
        val start = reader.cursor
        val yaw = AngleArgument.parse(reader, source, environment)
        if (!reader.canRead() || reader.peek() != ' ') {
            reader.cursor = start
            throw CommandSyntaxException(Component.translatable("argument.rotation.incomplete"), reader.string, start)
        }
        reader.skip()
        val pitch = AngleArgument.parse(reader, source, environment)
        return yaw to pitch
    }
}

/**
 * `minecraft:time`: a tick count, or a number with a unit (`10s`, `2d`, `40t`).
 *
 * parity: `TimeArgument`, whose units are ticks, seconds (20) and days (24000), and which rounds
 * the product down rather than to nearest.
 */
public class TimeArgument(
    private val min: Int = 0,
) : ArgumentType<Int> {
    override val wire: ArgumentParser = ArgumentParser.Time(min)

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): Int {
        val start = reader.cursor
        val value = reader.readDouble()
        val unit = if (reader.canRead() && reader.peek() in "dst") reader.read() else 't'
        val multiplier =
            when (unit) {
                'd' -> 24000
                's' -> 20
                else -> 1
            }
        val ticks = (value * multiplier).toInt()
        if (ticks < min) {
            reader.cursor = start
            throw CommandSyntaxException(
                Component.translatable(
                    "argument.time.tick_count_too_low",
                    Component.text(min.toString()),
                    Component.text(ticks.toString()),
                ),
                reader.string,
                reader.cursor,
            )
        }
        return ticks
    }
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

/**
 * A loaded world, by name.
 *
 * `brigadier:string` on the wire rather than `minecraft:dimension`: vanilla's dimension argument
 * resolves against the client's own dimension registry, and a Basalt world is not a dimension, it is
 * a level. Declaring it as a dimension would have the client reject names it has never heard of.
 */
public object WorldArgument : ArgumentType<org.basaltmc.server.runtime.world.LoadedWorld> {
    /**
     * parity: `minecraft:dimension`, which is what `/execute in` and `if dimension` declare.
     *
     * A world here is not always a dimension id, so the completions are this server's own world
     * names and the client is told to ask for them rather than offer its three.
     */
    override val wire: ArgumentParser = ArgumentParser.Dimension

    override val suggestions: SuggestionProvider = SuggestionProvider { it.matching(it.environment.worlds.keys) }

    override val clientSuggestions: String = SuggestionProviders.ASK_SERVER

    override fun parse(
        reader: StringReader,
        source: CommandSource,
        environment: CommandEnvironment,
    ): org.basaltmc.server.runtime.world.LoadedWorld {
        val start = reader.cursor
        val name = reader.readString()
        return environment.worlds[name] ?: run {
            reader.cursor = start
            throw CommandSyntaxException(
                Component.text(
                    "Unknown world '$name'. Loaded worlds: ${environment.worlds.keys.sorted().joinToString(", ")}.",
                ),
                reader.string,
                reader.cursor,
            )
        }
    }
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
 * `minecraft:entity` restricted to players, vanilla's `EntityArgument.players`.
 *
 * The same grammar as [EntitiesArgument] ([EntitySelector] parses it), so `@e[tag=admin]` here
 * means the players among them rather than a syntax error: vanilla's own `findPlayers` answers the
 * same selector the same way.
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
        val matched =
            EntitySelector
                .parse(reader, source, environment)
                .select(source, environment, playersOnly = true)
                .filterIsInstance<CommandTarget.Player>()
                .map { it.session }

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
