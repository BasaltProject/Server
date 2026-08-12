package org.basaltmc.server.runtime.config

import org.basaltmc.wire.common.logging.Logger
import org.basaltmc.wire.protocol.play.BossBarColor
import org.basaltmc.wire.protocol.play.BossBarOverlay

/** One bar. Anything left out is taken from the default profile. */
public data class BossBarProfile(
    private val header: ProfileHeader,
    public val text: String? = null,
    public val color: BossBarColor? = null,
    public val overlay: BossBarOverlay? = null,
    /** Kept as written so it can be a placeholder; resolved and clamped when the bar is drawn. */
    public val progress: String? = null,
) : ConditionalProfile by header {
    public fun inheriting(fallback: BossBarProfile): BossBarProfile =
        if (fallback === this) {
            this
        } else {
            copy(
                text = text ?: fallback.text,
                color = color ?: fallback.color,
                overlay = overlay ?: fallback.overlay,
                progress = progress ?: fallback.progress,
            )
        }
}

/** `config/boss_bar.toml`. */
public class BossBarConfig(
    public val enabled: Boolean,
    public val updateIntervalTicks: Int,
    public val profiles: List<BossBarProfile>,
    public val defaultProfile: BossBarProfile,
) {
    public companion object {
        public const val FILE_NAME: String = "boss_bar.toml"

        public val DISABLED: BossBarConfig =
            BossBarConfig(
                enabled = false,
                updateIntervalTicks = 0,
                profiles = emptyList(),
                defaultProfile =
                    BossBarProfile(ProfileHeader("default", 0, isDefault = true, isActive = false, emptyList())),
            )

        public fun load(
            directory: ConfigDirectory = ConfigDirectory(),
            logger: Logger = Logger("Config"),
        ): BossBarConfig {
            val root = directory.load(FILE_NAME, BOSS_BAR_DEFAULT_TOML)
            val path = directory.pathOf(FILE_NAME)
            val (profiles, default) =
                parseProfiles(root, path) { header, table ->
                    BossBarProfile(
                        header = header,
                        text = table.stringOrNull("text"),
                        color = table.stringOrNull("color")?.let { named(it, path, header.name, logger) },
                        overlay = table.stringOrNull("overlay")?.let { overlay(it, path, header.name, logger) },
                        progress = table.stringOrNull("progress") ?: table.intOrNull("progress")?.toString(),
                    )
                }
            return BossBarConfig(
                enabled = root.boolOrNull("enabled") ?: false,
                updateIntervalTicks = (root.intOrNull("update-interval-ticks")?.toInt() ?: 20).coerceAtLeast(0),
                profiles = profiles,
                defaultProfile = default,
            )
        }

        // An unknown colour is reported and falls back rather than stopping the server: it is one
        // wrong word in decoration, not a file that cannot be understood.
        private fun named(
            value: String,
            path: String,
            profile: String,
            logger: Logger,
        ): BossBarColor =
            BossBarColor.entries.firstOrNull { it.name.equals(value, ignoreCase = true) }
                ?: BossBarColor.WHITE.also {
                    logger.warn { "Boss bar profile '$profile' in $path has an unknown colour '$value'; using white." }
                }

        private fun overlay(
            value: String,
            path: String,
            profile: String,
            logger: Logger,
        ): BossBarOverlay =
            BossBarOverlay.entries.firstOrNull { it.name.equals(value, ignoreCase = true) }
                ?: BossBarOverlay.PROGRESS.also {
                    logger.warn {
                        "Boss bar profile '$profile' in $path has an unknown overlay '$value'; using progress."
                    }
                }
    }
}
