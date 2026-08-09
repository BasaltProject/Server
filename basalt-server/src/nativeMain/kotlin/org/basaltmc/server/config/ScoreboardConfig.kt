package org.basaltmc.server.config

import org.basaltmc.common.logging.Logger

/** One board: a title and its rows. Anything left out is taken from the default profile. */
public data class ScoreboardProfile(
    private val header: ProfileHeader,
    public val title: String? = null,
    public val lines: List<String>? = null,
) : ConditionalProfile by header {
    public fun inheriting(fallback: ScoreboardProfile): ScoreboardProfile =
        if (fallback === this) this else copy(title = title ?: fallback.title, lines = lines ?: fallback.lines)
}

/** `config/scoreboard.toml`. */
public class ScoreboardConfig(
    public val enabled: Boolean,
    public val updateIntervalTicks: Int,
    public val profiles: List<ScoreboardProfile>,
    public val defaultProfile: ScoreboardProfile,
) {
    public companion object {
        public const val FILE_NAME: String = "scoreboard.toml"

        /** The client only draws this many rows; more would be written and never seen. */
        public const val MAX_LINES: Int = 15

        public val DISABLED: ScoreboardConfig =
            ScoreboardConfig(
                enabled = false,
                updateIntervalTicks = 0,
                profiles = emptyList(),
                defaultProfile =
                    ScoreboardProfile(ProfileHeader("default", 0, isDefault = true, isActive = false, emptyList())),
            )

        public fun load(
            directory: ConfigDirectory = ConfigDirectory(),
            logger: Logger = Logger("Config"),
        ): ScoreboardConfig {
            val root = directory.load(FILE_NAME, SCOREBOARD_DEFAULT_TOML)
            val path = directory.pathOf(FILE_NAME)
            val (profiles, default) =
                parseProfiles(root, path) { header, table ->
                    ScoreboardProfile(header, table.stringOrNull("title"), table.linesOrNull("lines"))
                }
            for (profile in profiles) {
                val count = profile.lines?.size ?: 0
                if (count > MAX_LINES) {
                    logger.warn {
                        "Scoreboard profile '${profile.name}' in $path has $count lines; " +
                            "the client draws $MAX_LINES, so the rest are dropped."
                    }
                }
            }
            val enabled = root.boolOrNull("enabled") ?: false
            return ScoreboardConfig(
                enabled = enabled,
                updateIntervalTicks = (root.intOrNull("update-interval-ticks")?.toInt() ?: 20).coerceAtLeast(0),
                profiles = profiles,
                defaultProfile = default,
            )
        }
    }
}
