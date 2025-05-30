package org.basaltmc.server.config

import org.basaltmc.common.logging.Logger
import org.basaltmc.common.placeholder.ConditionResult
import org.basaltmc.common.placeholder.PlaceholderCondition
import org.basaltmc.common.placeholder.PlaceholderContext
import org.basaltmc.common.placeholder.PlaceholderRegistry
import kotlin.concurrent.AtomicReference

/**
 * What every profile in every UI file has in common: a name, a priority, whether it is switched on
 * by hand, and the conditions under which it applies.
 *
 * Extracted because the server list, the scoreboard, the tab list and the boss bar all select a
 * profile the same way, and four copies of "highest priority among those that apply" is four places
 * for the rule to drift apart. The files stay separate because they are separate domains; the
 * *rule* is one.
 */
public interface ConditionalProfile {
    public val name: String
    public val priority: Int
    public val isDefault: Boolean
    public val isActive: Boolean
    public val conditions: List<PlaceholderCondition>
}

/** The four shared keys, read out of a profile's table. */
public data class ProfileHeader(
    override val name: String,
    override val priority: Int,
    override val isDefault: Boolean,
    override val isActive: Boolean,
    override val conditions: List<PlaceholderCondition>,
) : ConditionalProfile {
    public companion object {
        public fun from(
            name: String,
            table: TomlValue.Table,
        ): ProfileHeader =
            ProfileHeader(
                name = name,
                priority = table.intOrNull("priority")?.toInt() ?: 0,
                isDefault = table.boolOrNull("default") ?: false,
                isActive = table.boolOrNull("active") ?: false,
                conditions = table.stringListOrNull("when").orEmpty().map(::PlaceholderCondition),
            )
    }
}

/**
 * Says a thing once, however many times it happens.
 *
 * UI profiles are re-selected on a timer for every player, so a condition that cannot be decided
 * would otherwise fill the log at the refresh rate. Held as an immutable set swapped by
 * compare-and-set because this is read from every connection thread at once.
 */
public class OnceReporter {
    private val said = AtomicReference<Set<String>>(emptySet())

    public fun report(
        logger: Logger,
        message: String,
    ) {
        while (true) {
            val current = said.value
            if (message in current) return
            if (said.compareAndSet(current, current + message)) {
                logger.warn { message }
                return
            }
        }
    }
}

/**
 * The profile a given situation gets: the highest priority among those switched on or whose every
 * condition holds, falling back to the one marked default.
 *
 * A condition that cannot be decided at all does not count as a no. The profile is skipped and the
 * reason is reported once, because "your condition is nonsense here" and "your condition said no"
 * are different things and only one of them is a mistake.
 */
public fun <T : ConditionalProfile> List<T>.selectProfile(
    fallback: T,
    registry: PlaceholderRegistry,
    context: PlaceholderContext,
    logger: Logger,
    reporter: OnceReporter,
    what: String,
): T =
    filter { it.isActive || it.applies(registry, context, logger, reporter, what) }
        .maxByOrNull { it.priority }
        ?: fallback

private fun ConditionalProfile.applies(
    registry: PlaceholderRegistry,
    context: PlaceholderContext,
    logger: Logger,
    reporter: OnceReporter,
    what: String,
): Boolean {
    if (conditions.isEmpty()) return false
    for (condition in conditions) {
        when (val result = condition.evaluate(registry, context)) {
            ConditionResult.True -> continue
            ConditionResult.False -> return false
            is ConditionResult.Unresolvable -> {
                reporter.report(
                    logger,
                    "Condition '${condition.raw}' of $what profile '$name' was skipped: ${result.reason}.",
                )
                return false
            }
        }
    }
    return true
}

/**
 * Reads the profiles of a UI file, checking the one rule they all share: exactly one default.
 *
 * [build] gets the header already parsed, so each file only writes out the keys that are its own.
 */
public fun <T : ConditionalProfile> parseProfiles(
    root: TomlValue.Table,
    path: String,
    build: (ProfileHeader, TomlValue.Table) -> T,
): Pair<List<T>, T> {
    val declared =
        root.tableOrNull("profiles")
            ?: throw TomlFormatException("$path declares no [profiles] table.")
    val profiles =
        declared.keys().mapNotNull { name ->
            declared.tableOrNull(name)?.let { build(ProfileHeader.from(name, it), it) }
        }
    if (profiles.isEmpty()) throw TomlFormatException("$path declares no profiles.")

    val defaults = profiles.filter { it.isDefault }
    if (defaults.size != 1) {
        throw TomlFormatException("$path must mark exactly one profile as `default`, found ${defaults.size}.")
    }
    return profiles to defaults.single()
}

/** A block of text as a file writes it: a list of lines, or one string split on its newlines. */
public fun TomlValue.Table.linesOrNull(key: String): List<String>? =
    stringListOrNull(key) ?: stringOrNull(key)?.split("\n")
