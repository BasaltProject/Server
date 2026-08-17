package org.basaltmc.server.runtime.command.commands

import org.basaltmc.server.random.LegacyRandom
import org.basaltmc.server.runtime.command.BoolArgument
import org.basaltmc.server.runtime.command.Command
import org.basaltmc.server.runtime.command.CommandContext
import org.basaltmc.server.runtime.command.CommandNodeBuilder
import org.basaltmc.server.runtime.command.CommandSyntaxException
import org.basaltmc.server.runtime.command.IntRangeArgument
import org.basaltmc.server.runtime.command.IntegerArgument
import org.basaltmc.server.runtime.command.PermissionLevel
import org.basaltmc.server.runtime.command.ResourceLocationArgument
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.text.Component

/**
 * parity: `RandomCommand`.
 *
 * Named sequences are what make a datapack's randomness reproducible: each one is its own stream,
 * seeded from the world seed and the name, and `/random reset` puts it back to its start. `value`
 * is the quiet form and `roll` is the one everybody sees.
 */
public class RandomCommand(
    private val worldSeed: () -> Long,
) : Command(name = "random", permissionLevel = PermissionLevel.ALL) {
    private val sequences = mutableMapOf<Identifier, LegacyRandom>()

    override fun build(root: CommandNodeBuilder) {
        for (kind in listOf("value", "roll")) {
            root.literal(kind) {
                argument("range", IntRangeArgument) {
                    executes { context -> draw(context, kind, null) }
                    argument("sequence", ResourceLocationArgument) {
                        suggestsFromServer { context -> context.matchingIds(sequences.keys) }
                        executes { context -> draw(context, kind, context.get("sequence")) }
                    }
                }
            }
        }
        root.literal("reset") {
            literal("*") {
                executes { context -> resetAll(context) }
                seeded { context -> resetAll(context) }
            }
            argument("sequence", ResourceLocationArgument) {
                suggestsFromServer { context -> context.matchingIds(sequences.keys) }
                executes { context -> reset(context) }
                seeded { context -> reset(context) }
            }
        }
    }

    /**
     * `seed <n> [includeWorldSeed] [includeSequenceId]`, which reseeds rather than forgets.
     *
     * Sequences here are forgotten and rebuilt from the world seed on the next draw, so a seed
     * given by hand resets them the same way: the flags decide what vanilla mixes in, and this
     * build has nothing else to mix.
     */
    private fun CommandNodeBuilder.seeded(run: (CommandContext) -> Int) {
        argument("seed", IntegerArgument()) {
            executes { context -> run(context) }
            argument("includeWorldSeed", BoolArgument) {
                executes { context -> run(context) }
                argument("includeSequenceId", BoolArgument) { executes { context -> run(context) } }
            }
        }
    }

    private fun draw(
        context: CommandContext,
        kind: String,
        sequence: Identifier?,
    ): Int {
        val (min, max) = parseRange(context.get("range"))
        if (min > max) throw CommandSyntaxException(Component.translatable("commands.random.error.range_too_small"))
        val span = max.toLong() - min + 1
        if (span > Int.MAX_VALUE) {
            throw CommandSyntaxException(Component.translatable("commands.random.error.range_too_large"))
        }
        val random = sequence?.let { sequenceFor(it) } ?: LegacyRandom(worldSeed() xor ANY_SEQUENCE_SALT)
        val value = min + random.nextInt(span.toInt())
        if (kind == "roll") {
            context.environment.broadcast(
                Component.translatable(
                    "commands.random.roll",
                    context.source.displayName,
                    Component.text(value.toString()),
                    Component.text(min.toString()),
                    Component.text(max.toString()),
                ),
            )
        } else {
            context.success(Component.translatable("commands.random.sample.success", Component.text(value.toString())))
        }
        return value
    }

    private fun reset(context: CommandContext): Int {
        val sequence = context.get<Identifier>("sequence")
        sequences.remove(sequence)
        context.success(
            Component.translatable("commands.random.reset.success", Component.text(sequence.toString())),
        )
        return 1
    }

    private fun resetAll(context: CommandContext): Int {
        val count = sequences.size
        sequences.clear()
        context.success(
            Component.translatable("commands.random.reset.all.success", Component.text(count.toString())),
        )
        return count
    }

    /** parity: `RandomSequences.get`, whose stream is seeded from the world seed and the name. */
    private fun sequenceFor(sequence: Identifier): LegacyRandom =
        sequences.getOrPut(sequence) { LegacyRandom(worldSeed() xor sequence.toString().hashCode().toLong()) }

    /** `<min>..<max>`, or a bare number, which vanilla reads as `1..n`. */
    private fun parseRange(text: String): Pair<Int, Int> {
        if (".." !in text) {
            val single = text.toIntOrNull() ?: throw CommandSyntaxException(Component.text("Invalid range '$text'."))
            return 1 to single
        }
        val min = text.substringBefore("..").takeIf { it.isNotEmpty() }?.toIntOrNull() ?: Int.MIN_VALUE
        val max = text.substringAfter("..").takeIf { it.isNotEmpty() }?.toIntOrNull() ?: Int.MAX_VALUE
        return min to max
    }

    private companion object {
        /** Keeps the unnamed stream from repeating the world's own sequence. */
        const val ANY_SEQUENCE_SALT = 0x5DEECE66DL
    }
}
