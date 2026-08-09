package org.basaltmc.server.config

import org.basaltmc.common.logging.Logger

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
