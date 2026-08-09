package org.basaltmc.server.config

import org.basaltmc.common.logging.Logger

internal const val ANIMATIONS_DEFAULT_TOML = """# Animations: text that changes on its own, shared by everything that redraws.
#
# An animation is a list of frames and how long each one lasts. It is used as a
# placeholder, %basalt_animation_<name>%, so the same one works in the scoreboard,
# the tab list, the boss bar and the MOTD without being defined four times.
#
# Which frame shows is worked out from the clock, not from a counter kept per
# player: everyone online sees the same frame at the same moment, and a player
# who joins halfway through does not start the animation again from the top.
#
# A frame is MiniMessage and can contain other placeholders, so an animation can
# wrap live data. It cannot contain another animation: that is the one thing that
# would loop forever.
#
# Nothing animates faster than the thing drawing it refreshes. The tab list
# refreshes every `update-interval-ticks` (20 ticks = 1 second by default), so a
# 100ms animation there still only changes once a second. Lower that interval if
# you want it smoother, and remember it is a packet per player per refresh.

[animations.website]
# The classic scrolling shine, one frame per position of the highlight.
change-interval-ms = 200
frames = [
    "<white>basaltmc.org</white>",
    "<gray>b</gray><white>asaltmc.org</white>",
    "<gray>ba</gray><white>saltmc.org</white>",
    "<gray>bas</gray><white>altmc.org</white>",
    "<gray>basa</gray><white>ltmc.org</white>",
    "<gray>basal</gray><white>tmc.org</white>",
    "<gray>basalt</gray><white>mc.org</white>",
    "<gray>basaltm</gray><white>c.org</white>",
    "<gray>basaltmc</gray><white>.org</white>",
    "<gray>basaltmc.</gray><white>org</white>",
    "<gray>basaltmc.o</gray><white>rg</white>",
    "<gray>basaltmc.or</gray><white>g</white>",
    "<gray>basaltmc.org</gray>",
]

[animations.dots]
# For a "loading" or "live" feel next to a number.
change-interval-ms = 500
frames = ["", ".", "..", "..."]
"""

/**
 * One animation: its frames, and how long each is shown.
 *
 * There is no state here on purpose. The frame is a function of the clock, which is what makes an
 * animation the same for everyone looking at it, costs nothing per player, and survives a player
 * reconnecting without restarting.
 */
public data class Animation(
    public val name: String,
    public val frames: List<String>,
    public val changeIntervalMillis: Long,
) {
    public fun frameAt(epochMillis: Long): String {
        if (frames.size == 1) return frames.single()
        val step = (epochMillis / changeIntervalMillis) % frames.size
        return frames[step.toInt()]
    }
}

/** `config/animations.toml`. */
public class AnimationsConfig(
    private val animations: Map<String, Animation>,
) {
    public val names: Set<String> get() = animations.keys

    public operator fun get(name: String): Animation? = animations[name.lowercase()]

    public companion object {
        public const val FILE_NAME: String = "animations.toml"

        /** Below this an animation would just burn packets: nothing redraws that fast. */
        private const val MINIMUM_INTERVAL_MILLIS = 50L

        public fun load(
            directory: ConfigDirectory = ConfigDirectory(),
            logger: Logger = Logger("Config"),
        ): AnimationsConfig {
            val root = directory.load(FILE_NAME, ANIMATIONS_DEFAULT_TOML)
            val path = directory.pathOf(FILE_NAME)
            val declared = root.tableOrNull("animations")

            val animations =
                declared
                    ?.keys()
                    ?.mapNotNull { name ->
                        val table = declared.tableOrNull(name) ?: return@mapNotNull null
                        val frames = table.stringListOrNull("frames").orEmpty()
                        if (frames.isEmpty()) {
                            logger.warn { "Animation '$name' in $path has no frames; skipping it." }
                            return@mapNotNull null
                        }
                        val interval =
                            (table.intOrNull("change-interval-ms") ?: 200L).coerceAtLeast(MINIMUM_INTERVAL_MILLIS)
                        name.lowercase() to Animation(name.lowercase(), frames, interval)
                    }?.toMap()
                    .orEmpty()

            if (animations.isNotEmpty()) {
                logger.debug { "Loaded ${animations.size} animation(s) from $path." }
            }
            return AnimationsConfig(animations)
        }
    }
}
