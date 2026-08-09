package org.basaltmc.server.config

import org.basaltmc.common.logging.Logger

internal const val TAB_LIST_DEFAULT_TOML = """# The header and footer of the player list, the panel you see holding Tab.
#
# JAVA ONLY. Bedrock has no player list to decorate, so none of this reaches a
# Bedrock player: they are not missing a feature Basalt failed to send, their
# game has nowhere to put it. Nothing here needs turning off for them.
#
# This is the one UI file switched on out of the box, because a header and a
# footer are decoration: they cannot cover the screen or get in the way. The
# scoreboard and the boss bar ship off for exactly that reason.
#
# Profiles work exactly as in server_list.toml: highest priority among those
# switched on with `active` or whose every `when` holds, falling back to the one
# marked `default`. Text is MiniMessage, one entry per line, with the same
# %placeholders% as everywhere else, animations included.

enabled = true

# %basalt_server_cpu% is a percentage of ONE core, the same as `top` shows, so it
# goes over 100 when more than one thread is busy.
#
# How often each player's list is redrawn, in ticks (20 = one second), and the
# ceiling on how smooth any animation in here can look. 0 sends it once on join
# and never again.
#
# One tick is what makes the animation below move. Lowering it is fine: a redraw
# only becomes a packet when the text it produces actually changed, so a static
# panel stays free however often it is drawn. What costs is animated text, at one
# packet per player per change. 1 is 20 a second per player; with a hundred
# players that is 2000 packets a second for this panel alone, which is affordable
# but no longer nothing.
update-interval-ticks = 1

[profiles.default]
default = true
header = [
    "                                                            ",
    "<bold><gradient:gray:white>BASALT SERVER</gradient></bold>",
    "<gray>%basalt_animation_website%</gray>",
    "                                                            ",
]
footer = [
    " ",
    "<gray>TPS <white>%basalt_server_tps%</white>   RAM <white>%basalt_server_ram_used%MB</white>   CPU <white>%basalt_server_cpu%%</white></gray>",
    "<gray>Players <white>%basalt_server_online%/%basalt_server_max_players%</white>   Chunks <white>%basalt_server_total_chunks%</white>   Up <white>%basalt_server_uptime%</white></gray>",
    " ",
]

# A second profile, to show the shape. Delete it or make it yours.
# [profiles.staff]
# priority = 10
# when = ["%basalt_player_is_op% = true"]
# header = ["<red>STAFF</red>"]
"""

/** One look for the player list. Anything left out is taken from the default profile. */
public data class TabListProfile(
    private val header: ProfileHeader,
    public val headerLines: List<String>? = null,
    public val footerLines: List<String>? = null,
) : ConditionalProfile by header {
    public fun inheriting(fallback: TabListProfile): TabListProfile =
        if (fallback === this) {
            this
        } else {
            copy(
                headerLines = headerLines ?: fallback.headerLines,
                footerLines = footerLines ?: fallback.footerLines,
            )
        }
}

/** `config/tab_list.toml`. */
public class TabListConfig(
    public val enabled: Boolean,
    public val updateIntervalTicks: Int,
    public val profiles: List<TabListProfile>,
    public val defaultProfile: TabListProfile,
) {
    public companion object {
        public const val FILE_NAME: String = "tab_list.toml"

        public val DISABLED: TabListConfig =
            TabListConfig(
                enabled = false,
                updateIntervalTicks = 0,
                profiles = emptyList(),
                defaultProfile =
                    TabListProfile(ProfileHeader("default", 0, isDefault = true, isActive = false, emptyList())),
            )

        public fun load(
            directory: ConfigDirectory = ConfigDirectory(),
            logger: Logger = Logger("Config"),
        ): TabListConfig {
            val root = directory.load(FILE_NAME, TAB_LIST_DEFAULT_TOML)
            val path = directory.pathOf(FILE_NAME)
            val (profiles, default) =
                parseProfiles(root, path) { header, table ->
                    TabListProfile(header, table.linesOrNull("header"), table.linesOrNull("footer"))
                }
            val enabled = root.boolOrNull("enabled") ?: true
            if (!enabled) logger.debug { "$path is switched off." }
            return TabListConfig(
                enabled = enabled,
                updateIntervalTicks = (root.intOrNull("update-interval-ticks")?.toInt() ?: 20).coerceAtLeast(0),
                profiles = profiles,
                defaultProfile = default,
            )
        }
    }
}
