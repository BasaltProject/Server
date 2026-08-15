package org.basaltmc.server.runtime.command

import org.basaltmc.server.runtime.world.LoadedWorld
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.protocol.play.CommandSuggestion

/**
 * What a command's completions are computed from: who is typing, the server around them, the text
 * of the argument so far, and whatever earlier arguments on the same line already parsed.
 *
 * The earlier arguments are the reason this is not just a string: `/advancement grant <who> only
 * <advancement> <criterion>` can only list the criteria of the advancement that was already named.
 */
public class SuggestionContext(
    public val source: CommandSource,
    public val dispatcher: CommandDispatcher,
    /** The argument's text so far, as typed. */
    public val remaining: String,
    private val arguments: Map<String, Any>,
) {
    public val environment: CommandEnvironment get() = dispatcher.environment

    /** The world the command would act on, or null when none is loaded. */
    public val world: LoadedWorld?
        get() =
            environment.worlds[source.player?.worldName]
                ?: environment.worlds[environment.defaultWorldName]
                ?: environment.worlds.values.firstOrNull()

    @Suppress("UNCHECKED_CAST")
    public fun <T> argument(name: String): T? = arguments[name] as? T

    /** parity: `SharedSuggestionProvider.suggest`, which matches at any `.`, `_` or `/` boundary. */
    public fun matching(values: Iterable<String>): List<CommandSuggestion> {
        val pattern = remaining.lowercase()
        return values
            .filter { matchesSubString(pattern, it.lowercase()) }
            .sorted()
            .map { CommandSuggestion(it) }
    }

    /**
     * parity: `SharedSuggestionProvider.filterResources`: with no `:` typed, the namespace and the
     * path are matched separately, so `vill` finds `minecraft:village_plains`.
     */
    public fun matchingIds(values: Iterable<Identifier>): List<CommandSuggestion> {
        val pattern = remaining.lowercase()
        val qualified = ':' in remaining
        return values
            .filter {
                if (qualified) {
                    matchesSubString(pattern, it.toString())
                } else {
                    matchesSubString(pattern, it.namespace) || matchesSubString(pattern, it.path)
                }
            }.map { it.toString() }
            .sorted()
            .map { CommandSuggestion(it) }
    }

    private fun matchesSubString(
        pattern: String,
        input: String,
    ): Boolean {
        var index = 0
        while (!input.startsWith(pattern, index)) {
            val splitter = input.indexOfAny(MATCH_SPLITTERS, index)
            if (splitter < 0) return false
            index = splitter + 1
        }
        return true
    }

    private companion object {
        val MATCH_SPLITTERS = charArrayOf('.', '_', '/')
    }
}

/** Where an argument's completions come from. parity: brigadier's `SuggestionProvider`. */
public fun interface SuggestionProvider {
    public fun suggest(context: SuggestionContext): List<CommandSuggestion>
}

/** The completions for one line, and the slice of it they replace. */
public class SuggestionResult(
    public val start: Int,
    public val length: Int,
    public val suggestions: List<CommandSuggestion>,
) {
    public companion object {
        public val EMPTY: SuggestionResult = SuggestionResult(0, 0, emptyList())
    }
}

/**
 * The vanilla suggestion providers the client resolves on its own.
 *
 * A node naming one of these costs no round trip: the client has the same registry and answers from
 * it. Anything else is [ASK_SERVER], which is what a server-side [SuggestionProvider] becomes.
 */
public object SuggestionProviders {
    public const val ASK_SERVER: String = "minecraft:ask_server"
    public const val AVAILABLE_SOUNDS: String = "minecraft:available_sounds"
    public const val SUMMONABLE_ENTITIES: String = "minecraft:summonable_entities"
}
