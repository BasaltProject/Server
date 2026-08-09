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
# Nothing animates faster than the thing drawing it refreshes, and nothing
# redraws faster than one tick (50ms). Asking for less than that is not wrong, it
# just means frames are skipped. Lower `update-interval-ticks` to see more of
# them, and remember every redraw that changes anything is a packet per player.

[animations.website]
# The classic scrolling shine, one frame per position of the highlight.
change-interval-ms = 100
frames = [
    "<gray>b</gray><dark_gray>asaltmc.org</dark_gray>",
    "<white>b</white><gray>a</gray><dark_gray>saltmc.org</dark_gray>",
    "<white>ba</white><gray>s</gray><dark_gray>altmc.org</dark_gray>",
    "<white>bas</white><gray>a</gray><dark_gray>ltmc.org</dark_gray>",
    "<gray>b</gray><white>asa</white><gray>l</gray><dark_gray>tmc.org</dark_gray>",
    "<dark_gray>b</dark_gray><gray>a</gray><white>sal</white><gray>t</gray><dark_gray>mc.org</dark_gray>",
    "<dark_gray>ba</dark_gray><gray>s</gray><white>alt</white><gray>m</gray><dark_gray>c.org</dark_gray>",
    "<dark_gray>bas</dark_gray><gray>a</gray><white>ltm</white><gray>c</gray><dark_gray>.org</dark_gray>",
    "<dark_gray>basa</dark_gray><gray>l</gray><white>tmc</white><gray>.</gray><dark_gray>org</dark_gray>",
    "<dark_gray>basal</dark_gray><gray>t</gray><white>mc.</white><gray>o</gray><dark_gray>rg</dark_gray>",
    "<dark_gray>basalt</dark_gray><gray>m</gray><white>c.o</white><gray>r</gray><dark_gray>g</dark_gray>",
    "<dark_gray>basaltm</dark_gray><gray>c</gray><white>.or</white><gray>g</gray>",
    "<dark_gray>basaltmc</dark_gray><gray>.</gray><white>org</white>",
    "<dark_gray>basaltmc.</dark_gray><gray>o</gray><white>rg</white>",
    "<dark_gray>basaltmc.o</dark_gray><gray>r</gray><white>g</white>",
    "<dark_gray>basaltmc.or</dark_gray><gray>g</gray>",
    "<dark_gray>basaltmc.org</dark_gray>",
    "<dark_gray>basaltmc.org</dark_gray>",
    "<dark_gray>basaltmc.org</dark_gray>",
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

        /**
         * A floor low enough not to silently rewrite what the file asks for.
         *
         * It used to be 50 (one tick, the fastest anything can redraw), which quietly turned a
         * `change-interval-ms = 20` into 50: the file said one thing and the server did another.
         * Asking for less than a tick is a legitimate choice, it just skips frames.
         */
        private const val MINIMUM_INTERVAL_MILLIS = 1L

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
