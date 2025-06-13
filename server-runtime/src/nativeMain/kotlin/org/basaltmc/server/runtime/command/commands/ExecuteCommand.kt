package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.runtime.command.BlockPosArgument
import org.basaltmc.server.runtime.command.BlockPredicateArgument
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSource
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.ExecutionSource
import org.basaltmc.server.runtime.command.GreedyStringArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.PlayersArgument
import org.basaltmc.server.runtime.command.ResourceLocationArgument
import org.basaltmc.server.runtime.command.StringReader
import org.basaltmc.server.runtime.command.Vec3Argument
import org.basaltmc.server.runtime.command.WorldArgument
import org.basaltmc.server.runtime.command.worldIn
import org.basaltmc.server.runtime.player.PlayerSession
import org.basaltmc.server.runtime.scoreboard.Scoreboard
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.text.Component
import kotlin.math.floor

/**
 * parity: `ExecuteCommand`.
 *
 * The chain is one greedy argument rather than a redirecting tree: this dispatcher's redirects
 * change which node parses next, not what an earlier link's arguments were, so a declared chain
 * would keep only its last modifier. Cost: no client autocompletion past `/execute`.
 *
 * Missing, each for want of a subsystem: `facing`, `if blocks|data|predicate|items`, and `store`
 * into anything but a score.
 */
public class ExecuteCommand(
    private val scoreboard: Scoreboard,
    /** Where `store ... bossbar` writes, or null in a fixture with no bars. */
    private val bossBars: BossBarRegistry? = null,
    /** Where `store ... storage` writes and `if data` reads, or null in a fixture without it. */
    private val data: DataAccess? = null,
) : Command(name = "execute", permissionLevel = PermissionLevel.GAMEMASTERS) {
    override fun build(root: CommandNodeBuilder) {
        root.argument("chain", GreedyStringArgument) {
            executes { context -> execute(context, StringReader(context.get("chain"))) }
        }
    }

    /** One link's state: who it runs as, where, and what it should do with the result. */
    private class Chain(
        val sources: List<CommandSource>,
        val stores: List<Store> = emptyList(),
    )

    /** Where a result goes when the chain finishes. parity: `/execute store`'s three destinations. */
    private sealed interface Store {
        val fromSuccess: Boolean

        class Score(
            val holders: List<String>,
            val objective: String,
            override val fromSuccess: Boolean,
        ) : Store

        class BossBar(
            val id: Identifier,
            val max: Boolean,
            override val fromSuccess: Boolean,
        ) : Store

        class Data(
            val storage: Identifier,
            val path: String,
            val type: String,
            val scale: Double,
            override val fromSuccess: Boolean,
        ) : Store

        class BlockData(
            val pos: Triple<Int, Int, Int>,
            val path: String,
            val type: String,
            val scale: Double,
            override val fromSuccess: Boolean,
        ) : Store
    }

    private fun execute(
        context: CommandContext,
        reader: StringReader,
    ): Int {
        var chain = Chain(listOf(context.source))
        while (true) {
            reader.skipWhitespace()
            if (!reader.canRead()) throw reader.syntaxError("Expected a subcommand")
            val keyword = reader.readUnquotedString()
            when (keyword) {
                "run" -> return run(context, chain, reader)
                "as" -> chain = withEach(context, chain, reader) { source, target -> asEntity(source, target) }
                "at" -> chain = withEach(context, chain, reader) { source, target -> atEntity(source, target) }
                "positioned" -> chain = positioned(context, chain, reader)
                "rotated" -> chain = rotated(context, chain, reader)
                "facing" -> chain = facing(context, chain, reader)
                "in" -> chain = inWorld(context, chain, reader)
                "align" -> chain = align(chain, reader)
                "anchored" -> chain = anchored(chain, reader)
                "if", "unless" -> {
                    val outcome = condition(context, chain, reader, negated = keyword == "unless")
                    chain = outcome.chain ?: return outcome.result
                }
                "store" -> chain = store(context, chain, reader)
                else -> {
                    reader.cursor -= keyword.length
                    throw reader.syntaxError("Unknown subcommand '$keyword'")
                }
            }
            if (chain.sources.isEmpty()) return 0
        }
    }

    private fun run(
        context: CommandContext,
        chain: Chain,
        reader: StringReader,
    ): Int {
        reader.skipWhitespace()
        val command = reader.remaining
        if (command.isBlank()) throw reader.syntaxError("Expected a command to run")
        var successes = 0
        var total = 0
        for (source in chain.sources) {
            val result = context.dispatcher.execute(source, command)
            if (result != 0) successes++
            total += result
        }
        applyStores(context, chain, successes, total)
        return total
    }

    private fun applyStores(
        context: CommandContext,
        chain: Chain,
        successes: Int,
        total: Int,
    ) {
        for (store in chain.stores) {
            val value = if (store.fromSuccess) successes else total
            when (store) {
                is Store.Score -> for (holder in store.holders) scoreboard.setScore(holder, store.objective, value)
                is Store.BossBar -> {
                    val bar = bossBars?.get(store.id) ?: continue
                    if (store.max) bar.max = maxOf(1, value) else bar.value = value
                }
                is Store.Data -> data?.store(store.storage, store.path, (value * store.scale), store.type)
                is Store.BlockData ->
                    data?.storeInBlock(context, store.pos, store.path, value * store.scale, store.type)
            }
        }
    }

    private inline fun withEach(
        context: CommandContext,
        chain: Chain,
        reader: StringReader,
        transform: (CommandSource, PlayerSession) -> CommandSource,
    ): Chain {
        val targets = readPlayers(context, reader)
        val sources = chain.sources.flatMap { source -> targets.map { transform(source, it) } }
        return Chain(sources, chain.stores)
    }

    private fun asEntity(
        source: CommandSource,
        target: PlayerSession,
    ): CommandSource = ExecutionSource(source, player = target)

    private fun atEntity(
        source: CommandSource,
        target: PlayerSession,
    ): CommandSource =
        ExecutionSource(
            source,
            position = Triple(target.x, target.y, target.z),
            rotation = target.yaw to target.pitch,
        )

    private fun positioned(
        context: CommandContext,
        chain: Chain,
        reader: StringReader,
    ): Chain {
        reader.skipWhitespace()
        if (reader.remaining.startsWith("as ")) {
            reader.cursor += "as".length
            return withEach(context, chain, reader) { source, target ->
                ExecutionSource(source, position = Triple(target.x, target.y, target.z))
            }
        }
        val sources =
            chain.sources.map { source ->
                val coordinates = Vec3Argument.parse(reader, source, context.environment)
                ExecutionSource(source, position = coordinates.resolve(source.position))
            }
        return Chain(sources, chain.stores)
    }

    private fun rotated(
        context: CommandContext,
        chain: Chain,
        reader: StringReader,
    ): Chain {
        reader.skipWhitespace()
        if (reader.remaining.startsWith("as ")) {
            reader.cursor += "as".length
            return withEach(context, chain, reader) { source, target ->
                ExecutionSource(source, rotation = target.yaw to target.pitch)
            }
        }
        val yaw = reader.readDouble().toFloat()
        reader.expect(' ')
        val pitch = reader.readDouble().toFloat()
        return Chain(chain.sources.map { ExecutionSource(it, rotation = yaw to pitch) }, chain.stores)
    }

    /**
     * `facing <pos>` and `facing entity <targets> <anchor>`: turns each source to look at
     * something, which is what a `run tp ~ ~ ~ ~ ~` or a positioned raycast then uses.
     */
    private fun facing(
        context: CommandContext,
        chain: Chain,
        reader: StringReader,
    ): Chain {
        reader.skipWhitespace()
        if (reader.remaining.startsWith("entity ")) {
            reader.cursor += "entity".length
            val targets = readPlayers(context, reader)
            reader.skipWhitespace()
            // The anchor is required after the selector, and only changes where we aim.
            val anchor = if (reader.canRead() && reader.peek() != ' ') reader.readUnquotedString() else "feet"
            val eyes = anchor == "eyes"
            val sources =
                chain.sources.flatMap { source ->
                    targets.map { target ->
                        val targetY = target.y + if (eyes) EYE_HEIGHT else 0.0
                        val (yaw, pitch) = anglesFrom(source, target.x, targetY, target.z)
                        ExecutionSource(source, rotation = yaw to pitch)
                    }
                }
            return Chain(sources, chain.stores)
        }
        val sources =
            chain.sources.map { source ->
                val coordinates = Vec3Argument.parse(reader, source, context.environment)
                val (x, y, z) = coordinates.resolve(source.position)
                val (yaw, pitch) = anglesFrom(source, x, y, z)
                ExecutionSource(source, rotation = yaw to pitch)
            }
        return Chain(sources, chain.stores)
    }

    /** parity: `Entity.lookAt`: yaw grows clockwise from +Z, pitch is positive downwards. */
    private fun anglesFrom(
        source: CommandSource,
        x: Double,
        y: Double,
        z: Double,
    ): Pair<Float, Float> {
        val (sourceX, sourceY, sourceZ) = source.position
        val eyeY = sourceY + if ((source as? ExecutionSource)?.anchorEyes == true) EYE_HEIGHT else 0.0
        val dx = x - sourceX
        val dy = y - eyeY
        val dz = z - sourceZ
        val horizontal = kotlin.math.sqrt(dx * dx + dz * dz)
        val yaw = (-kotlin.math.atan2(dx, dz) * DEGREES_PER_RADIAN).toFloat()
        val pitch = (-kotlin.math.atan2(dy, horizontal) * DEGREES_PER_RADIAN).toFloat()
        return yaw to pitch
    }

    private fun inWorld(
        context: CommandContext,
        chain: Chain,
        reader: StringReader,
    ): Chain {
        reader.skipWhitespace()
        val world = WorldArgument.parse(reader, context.source, context.environment)
        return Chain(chain.sources.map { ExecutionSource(it, world = world) }, chain.stores)
    }

    /** parity: `align`, which floors the named axes so a position snaps to its block corner. */
    private fun align(
        chain: Chain,
        reader: StringReader,
    ): Chain {
        reader.skipWhitespace()
        val axes = reader.readUnquotedString()
        if (axes.isEmpty() || axes.any { it !in "xyz" } || axes.toSet().size != axes.length) {
            throw reader.syntaxError("Invalid swizzle, expected combination of 'x', 'y' and 'z'")
        }
        val sources =
            chain.sources.map { source ->
                val (x, y, z) = source.position
                ExecutionSource(
                    source,
                    position =
                        Triple(
                            if ('x' in axes) floor(x) else x,
                            if ('y' in axes) floor(y) else y,
                            if ('z' in axes) floor(z) else z,
                        ),
                )
            }
        return Chain(sources, chain.stores)
    }

    private fun anchored(
        chain: Chain,
        reader: StringReader,
    ): Chain {
        reader.skipWhitespace()
        val anchor = reader.readUnquotedString()
        if (anchor != "eyes" && anchor != "feet") throw reader.syntaxError("Invalid entity anchor")
        return Chain(chain.sources.map { ExecutionSource(it, anchorEyes = anchor == "eyes") }, chain.stores)
    }

    /**
     * What `if`/`unless` produced: the chain to carry on with, or null with the result of a
     * condition that ended the line, which is what `/execute if ...` on its own reports.
     */
    private class Conditioned(
        val chain: Chain?,
        val result: Int = 0,
    )

    private fun condition(
        context: CommandContext,
        chain: Chain,
        reader: StringReader,
        negated: Boolean,
    ): Conditioned {
        reader.skipWhitespace()
        val kind = reader.readUnquotedString()
        lastConditionCount = null
        val surviving =
            chain.sources.filter { source ->
                val matched = test(context, source, kind, reader, chain.sources.first() === source)
                matched != negated
            }
        reader.skipWhitespace()
        if (!reader.canRead()) {
            // The condition was the last thing on the line, so it reports rather than gates.
            val count = if (surviving.isNotEmpty()) lastConditionCount ?: surviving.size else 0
            lastConditionCount = null
            context.success(
                if (count > 0) {
                    Component.translatable("commands.execute.conditional.pass_count", Component.text(count.toString()))
                } else {
                    Component.translatable("commands.execute.conditional.fail")
                },
            )
            applyStores(context, chain, if (count > 0) 1 else 0, count)
            return Conditioned(null, result = count)
        }
        return Conditioned(Chain(surviving, chain.stores))
    }

    /**
     * One condition against one source. [consume] says whether this call is the one that advances
     * the reader: the condition text is shared by every source, so only the first parse moves it.
     */
    private fun test(
        context: CommandContext,
        source: CommandSource,
        kind: String,
        reader: StringReader,
        consume: Boolean,
    ): Boolean {
        val start = reader.cursor
        val result =
            when (kind) {
                "block" -> {
                    reader.skipWhitespace()
                    val coordinates = BlockPosArgument.parse(reader, source, context.environment)
                    reader.skipWhitespace()
                    val predicate = BlockPredicateArgument.parse(reader, source, context.environment)
                    val world = source.worldIn(context.environment)
                    val (x, y, z) = coordinates.resolve(source.position)
                    val state = world?.state?.getBlockState(x, y, z)
                    state != null && predicate.matches(state)
                }
                "entity" -> {
                    reader.skipWhitespace()
                    runCatching { PlayersArgument(single = false).parse(reader, source, context.environment) }
                        .getOrElse { emptyList() }
                        .isNotEmpty()
                }
                "loaded" -> {
                    reader.skipWhitespace()
                    val coordinates = BlockPosArgument.parse(reader, source, context.environment)
                    val (x, _, z) = coordinates.resolve(source.position)
                    source.worldIn(context.environment)?.state?.isLoaded(x shr 4, z shr 4) == true
                }
                "dimension" -> {
                    reader.skipWhitespace()
                    val world = WorldArgument.parse(reader, source, context.environment)
                    source.worldIn(context.environment)?.name == world.name
                }
                "score" -> testScore(context, source, reader)
                "data" -> testData(context, source, reader)
                "blocks" -> testBlocks(context, source, reader)
                else -> {
                    reader.cursor = start
                    throw reader.syntaxError("Unsupported condition '$kind'")
                }
            }
        if (!consume) reader.cursor = start
        return result
    }

    /**
     * `if blocks <start> <end> <destination> (all|masked)`: compares two regions block by block.
     *
     * parity: `all` requires every position to match, `masked` ignores air in the source, and the
     * result is the number of matching blocks rather than 1, which is what makes
     * `/execute store result ... if blocks` useful.
     */
    private fun testBlocks(
        context: CommandContext,
        source: CommandSource,
        reader: StringReader,
    ): Boolean {
        reader.skipWhitespace()
        val start = BlockPosArgument.parse(reader, source, context.environment).resolve(source.position)
        reader.skipWhitespace()
        val end = BlockPosArgument.parse(reader, source, context.environment).resolve(source.position)
        reader.skipWhitespace()
        val destination = BlockPosArgument.parse(reader, source, context.environment).resolve(source.position)
        reader.skipWhitespace()
        val mode = reader.readUnquotedString().ifEmpty { "all" }
        if (mode != "all" && mode != "masked") throw reader.syntaxError("Expected 'all' or 'masked'")

        val world = source.worldIn(context.environment) ?: return false
        val minX = minOf(start.first, end.first)
        val minY = minOf(start.second, end.second)
        val minZ = minOf(start.third, end.third)
        val maxX = maxOf(start.first, end.first)
        val maxY = maxOf(start.second, end.second)
        val maxZ = maxOf(start.third, end.third)

        var matched = 0
        for (x in minX..maxX) {
            for (y in minY..maxY) {
                for (z in minZ..maxZ) {
                    val sourceState = world.state.getBlockState(x, y, z) ?: return false
                    if (mode == "masked" && sourceState == Blocks.AIR.stateId) continue
                    val targetState =
                        world.state.getBlockState(
                            destination.first + (x - minX),
                            destination.second + (y - minY),
                            destination.third + (z - minZ),
                        ) ?: return false
                    if (sourceState != targetState) return false
                    matched++
                }
            }
        }
        lastConditionCount = matched
        return true
    }

    /**
     * How many things the last condition matched, which `if blocks` reports instead of 1. Reset by
     * every condition that does not set it.
     */
    private var lastConditionCount: Int? = null

    /** `if data (block <pos>|entity <target>|storage <id>) <path>`, true when the path exists. */
    private fun testData(
        context: CommandContext,
        source: CommandSource,
        reader: StringReader,
    ): Boolean {
        reader.skipWhitespace()
        val holder = reader.readUnquotedString()
        reader.skipWhitespace()
        val query =
            when (holder) {
                "storage" -> {
                    val id = ResourceLocationArgument.parse(reader, source, context.environment)
                    reader.skipWhitespace()
                    val path = reader.readUnquotedString()
                    data?.pathExists(context, DataHolderRef.Storage(id), path) ?: false
                }
                "block" -> {
                    val pos = BlockPosArgument.parse(reader, source, context.environment)
                    reader.skipWhitespace()
                    val path = reader.readUnquotedString()
                    data?.pathExists(context, DataHolderRef.Block(pos.resolve(source.position)), path) ?: false
                }
                "entity" -> {
                    val targets = PlayersArgument(single = true).parse(reader, source, context.environment)
                    reader.skipWhitespace()
                    val path = reader.readUnquotedString()
                    data?.pathExists(context, DataHolderRef.Entity(targets.single()), path) ?: false
                }
                else -> throw reader.syntaxError("Expected 'block', 'entity' or 'storage'")
            }
        return query
    }

    /** `if score <holder> <objective> (matches <range> | (<|<=|=|>=|>) <holder> <objective>)`. */
    private fun testScore(
        context: CommandContext,
        source: CommandSource,
        reader: StringReader,
    ): Boolean {
        reader.skipWhitespace()
        val holder = readHolder(context, source, reader)
        reader.skipWhitespace()
        val objective = reader.readUnquotedString()
        val value = scoreboard.score(holder, objective)
        reader.skipWhitespace()
        val operator = reader.readUnquotedString().ifEmpty { readOperator(reader) }
        if (operator == "matches") {
            reader.skipWhitespace()
            val range = reader.readUnquotedString()
            return value != null && matchesRange(value, range, reader)
        }
        reader.skipWhitespace()
        val otherHolder = readHolder(context, source, reader)
        reader.skipWhitespace()
        val otherObjective = reader.readUnquotedString()
        val other = scoreboard.score(otherHolder, otherObjective)
        if (value == null || other == null) return false
        return when (operator) {
            "<" -> value < other
            "<=" -> value <= other
            "=" -> value == other
            ">=" -> value >= other
            ">" -> value > other
            else -> throw reader.syntaxError("Invalid score comparison '$operator'")
        }
    }

    private fun readOperator(reader: StringReader): String {
        val start = reader.cursor
        while (reader.canRead() && reader.peek() in "<>=") reader.skip()
        return reader.string.substring(start, reader.cursor)
    }

    private fun readHolder(
        context: CommandContext,
        source: CommandSource,
        reader: StringReader,
    ): String {
        if (reader.canRead() && reader.peek() == '@') {
            return PlayersArgument(single = true)
                .parse(reader, source, context.environment)
                .single()
                .name
        }
        val start = reader.cursor
        while (reader.canRead() && !reader.peek().isWhitespace()) reader.skip()
        return reader.string.substring(start, reader.cursor)
    }

    /** parity: `MinMaxBounds.Ints`, whose forms are `n`, `a..b`, `a..` and `..b`. */
    private fun matchesRange(
        value: Int,
        range: String,
        reader: StringReader,
    ): Boolean {
        if (".." !in range) {
            val exact = range.toIntOrNull() ?: throw reader.syntaxError("Invalid integer range '$range'")
            return value == exact
        }
        val min = range.substringBefore("..").takeIf { it.isNotEmpty() }?.toIntOrNull()
        val max = range.substringAfter("..").takeIf { it.isNotEmpty() }?.toIntOrNull()
        return (min == null || value >= min) && (max == null || value <= max)
    }

    /** `store (result|success) score <holders> <objective>`. */
    private fun store(
        context: CommandContext,
        chain: Chain,
        reader: StringReader,
    ): Chain {
        reader.skipWhitespace()
        val which = reader.readUnquotedString()
        if (which != "result" && which != "success") {
            throw reader.syntaxError("Expected 'result' or 'success'")
        }
        reader.skipWhitespace()
        val target = reader.readUnquotedString()
        val fromSuccess = which == "success"
        return when (target) {
            "score" -> {
                reader.skipWhitespace()
                val holder = readHolder(context, chain.sources.first(), reader)
                reader.skipWhitespace()
                val objective = reader.readUnquotedString()
                Chain(chain.sources, chain.stores + Store.Score(listOf(holder), objective, fromSuccess))
            }
            "bossbar" -> {
                reader.skipWhitespace()
                val id = ResourceLocationArgument.parse(reader, chain.sources.first(), context.environment)
                reader.skipWhitespace()
                val field = reader.readUnquotedString()
                if (field != "value" && field != "max") throw reader.syntaxError("Expected 'value' or 'max'")
                Chain(chain.sources, chain.stores + Store.BossBar(id, field == "max", fromSuccess))
            }
            "storage" -> {
                reader.skipWhitespace()
                val id = ResourceLocationArgument.parse(reader, chain.sources.first(), context.environment)
                reader.skipWhitespace()
                val path = reader.readUnquotedString()
                reader.skipWhitespace()
                // The value type and scale, which vanilla requires here.
                val type = reader.readUnquotedString()
                reader.skipWhitespace()
                val scale = if (reader.canRead() && reader.peek() != ' ') reader.readDouble() else 1.0
                Chain(chain.sources, chain.stores + Store.Data(id, path, type, scale, fromSuccess))
            }
            "block" -> {
                reader.skipWhitespace()
                val pos =
                    BlockPosArgument
                        .parse(reader, chain.sources.first(), context.environment)
                        .resolve(chain.sources.first().position)
                reader.skipWhitespace()
                val path = reader.readUnquotedString()
                reader.skipWhitespace()
                val type = reader.readUnquotedString()
                reader.skipWhitespace()
                val scale = if (reader.canRead() && reader.peek() != ' ') reader.readDouble() else 1.0
                Chain(chain.sources, chain.stores + Store.BlockData(pos, path, type, scale, fromSuccess))
            }
            else ->
                throw CommandSyntaxException(
                    Component.text(
                        "`/execute store` writes to a score, a bossbar or command storage in this build; " +
                            "block and entity data need holders that are not writable yet.",
                    ),
                    reader.string,
                    reader.cursor,
                )
        }
    }

    private fun readPlayers(
        context: CommandContext,
        reader: StringReader,
    ): List<PlayerSession> {
        reader.skipWhitespace()
        return runCatching {
            PlayersArgument(single = false).parse(reader, context.source, context.environment)
        }.getOrElse { emptyList() }
    }

    private companion object {
        const val DEGREES_PER_RADIAN = 57.2957795130823

        /** parity: a player's eye height, which is what `anchored eyes` aims from. */
        const val EYE_HEIGHT = 1.62
    }
}
