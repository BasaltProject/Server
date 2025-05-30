package org.basaltmc.server.config

import org.basaltmc.common.logging.Logger

internal const val SCOREBOARD_DEFAULT_TOML = """# The panel down the right-hand side of the screen.
#
# Off by default, and deliberately: it covers part of the view, so it is the kind
# of thing an operator opts into rather than something a fresh server imposes.
#
# Profiles work exactly as in server_list.toml: highest priority among those
# switched on with `active` or whose every `when` holds, falling back to the one
# marked `default`. Text is MiniMessage with the usual %placeholders%, animations
# included, and each entry in `lines` is one row.
#
# A line that would repeat another is quietly given a different invisible key, so
# two identical lines are allowed; the client keys rows by their owner, not their
# text.
#
# Any number takes a `_<decimals>` suffix: %basalt_player_x% is the true position
# and unreadable (21.70704236703265), %basalt_player_x_1% is 21.7.

enabled = false

# How often each player's board is redrawn, in ticks (20 = one second).
update-interval-ticks = 20

[profiles.default]
default = true
title = "<bold><gradient:gray:white>BASALT</gradient></bold>"
lines = [
    "",
    "<gray>World</gray> <white>%basalt_player_world%</white>",
    "<gray>Position</gray> <white>%basalt_player_x_1%, %basalt_player_y_1%, %basalt_player_z_1%</white>",
    "",
    "<gray>Online</gray> <white>%basalt_server_online%/%basalt_server_max_players%</white>",
    "<gray>TPS</gray> <white>%basalt_server_tps%</white>",
    "",
    "<gray>%basalt_animation_website%</gray>",
]

# Per dimension. Only the overworld exists in this build, so the two below never
# match yet: they are here to show the shape, and they will start working on
# their own once the Nether and the End are generated.
[profiles.nether]
priority = 10
when = ["%basalt_player_dimension% = the_nether"]
title = "<bold><red>NETHER</red></bold>"
lines = [
    "",
    "<gray>Position</gray> <white>%basalt_player_x_1%, %basalt_player_y_1%, %basalt_player_z_1%</white>",
    "<gray>Overworld</gray> <white>divide by 8</white>",
    "",
    "<gray>%basalt_animation_website%</gray>",
]

[profiles.end]
priority = 10
when = ["%basalt_player_dimension% = the_end"]
title = "<bold><light_purple>THE END</light_purple></bold>"
lines = [
    "",
    "<gray>Position</gray> <white>%basalt_player_x_1%, %basalt_player_y_1%, %basalt_player_z_1%</white>",
    "",
    "<gray>%basalt_animation_website%</gray>",
]
"""

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
