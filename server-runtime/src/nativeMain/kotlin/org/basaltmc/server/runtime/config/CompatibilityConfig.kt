package org.basaltmc.server.runtime.config

import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.common.toml.TomlFormatException
import org.basaltmc.wire.multiversion.ProtocolGate
import org.basaltmc.wire.multiversion.ProtocolVersions
import org.basaltmc.wire.multiversion.UnknownProtocolPolicy

/** Which older versions to accept: everything this build can translate, or a written list. */
public sealed interface BackwardsSelection {
    public data object All : BackwardsSelection

    public data class Only(
        public val protocols: Set<Int>,
    ) : BackwardsSelection
}

/** `config/compatibility.toml`. */
public class CompatibilityConfig(
    public val backwards: BackwardsSelection,
    public val futureProtocols: UnknownProtocolPolicy,
    public val snapshots: UnknownProtocolPolicy,
    /** Whether what stands in for content a client is too old for says what it really is. */
    public val nameSubstitutes: Boolean = true,
) {
    /**
     * The door, built from what this build can actually translate.
     *
     * [translatable] is what the chain of translators covers, so `all` grows on its own when a link
     * is added and an operator who wrote a list gets exactly the list, minus anything this build
     * cannot do, which is said out loud rather than accepted and then failed at.
     */
    public fun gate(
        translatable: Set<Int>,
        logger: Logger = Logger("Compatibility"),
    ): ProtocolGate {
        val supported =
            when (val selection = backwards) {
                BackwardsSelection.All -> translatable + ProtocolVersions.NATIVE
                is BackwardsSelection.Only -> {
                    val missing = selection.protocols - translatable - ProtocolVersions.NATIVE
                    if (missing.isNotEmpty()) {
                        logger.warn {
                            "compatibility.toml asks for protocols this build cannot translate: " +
                                missing.sorted().joinToString(", ") { ProtocolVersions.describe(it) }
                        }
                    }
                    (selection.protocols intersect (translatable + ProtocolVersions.NATIVE)) + ProtocolVersions.NATIVE
                }
            }
        return ProtocolGate(supported, futureProtocols, snapshots)
    }

    public companion object {
        public const val FILE_NAME: String = "compatibility.toml"

        public fun load(
            directory: ConfigDirectory = ConfigDirectory(),
            logger: Logger = Logger("Config"),
        ): CompatibilityConfig {
            val root = directory.load(FILE_NAME, COMPATIBILITY_DEFAULT_TOML)
            val path = directory.pathOf(FILE_NAME)
            val java = root.tableOrNull("java")

            val backwards =
                when (val written = java?.stringOrNull("backwards")?.trim()?.lowercase()) {
                    null ->
                        java?.intListOrNull("backwards")?.let { BackwardsSelection.Only(it.toIntSet()) }
                            ?: BackwardsSelection.All
                    "all" -> BackwardsSelection.All
                    // Not a guess: "backwards = none" and "backwards = 775" are both someone meaning
                    // something specific, and quietly reading either as "all" is the worst answer.
                    else -> throw TomlFormatException("$path: backwards must be \"all\" or a list of protocol numbers")
                }

            val future = policy(java?.stringOrNull("future-protocols"), "future-protocols", path)
            val snapshots = policy(java?.stringOrNull("snapshots"), "snapshots", path)
            val nameSubstitutes = java?.boolOrNull("name-substitutes") ?: true
            logger.debug { "Loaded $path." }
            return CompatibilityConfig(backwards, future, snapshots, nameSubstitutes)
        }

        private fun policy(
            written: String?,
            key: String,
            path: String,
        ): UnknownProtocolPolicy {
            val value = written ?: return UnknownProtocolPolicy.REFUSE
            return UnknownProtocolPolicy.parse(value)
                ?: throw TomlFormatException("$path: $key must be accept, warn or refuse, not \"$value\"")
        }

        private fun List<Long>.toIntSet(): Set<Int> = mapTo(mutableSetOf()) { it.toInt() }
    }
}
