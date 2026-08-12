package org.basaltmc.server.runtime.config

import org.basaltmc.wire.common.logging.Logger

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
