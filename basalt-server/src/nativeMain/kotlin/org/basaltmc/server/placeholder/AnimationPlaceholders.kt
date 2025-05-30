package org.basaltmc.server.placeholder

import org.basaltmc.common.placeholder.PlaceholderCatalog
import org.basaltmc.common.placeholder.PlaceholderContext
import org.basaltmc.common.placeholder.PlaceholderProvider
import org.basaltmc.common.placeholder.PlaceholderRegistry
import org.basaltmc.common.placeholder.PlaceholderText
import org.basaltmc.common.placeholder.PlaceholderValue
import org.basaltmc.server.config.AnimationsConfig
import kotlin.time.Clock
import kotlin.time.ExperimentalTime

/**
 * `%basalt_animation_<name>%`: the frame of that animation showing right now.
 *
 * Which frame comes from the wall clock, so two players looking at the same tab list see the same
 * frame, and nothing has to be stored per player or ticked.
 *
 * A frame may contain other placeholders and they are resolved here, which is what lets an animation
 * wrap live data. Animations inside animations are the one thing refused, and refused *before*
 * substituting rather than by catching it afterwards: the registry would resolve the inner one
 * happily, and two frames naming each other would recurse until the stack ran out.
 */
@OptIn(ExperimentalTime::class)
public class AnimationPlaceholders(
    private val animations: () -> AnimationsConfig,
    private val registry: () -> PlaceholderRegistry,
) : PlaceholderProvider,
    PlaceholderCatalog {
    override fun names(): Collection<String> = animations().names.sorted()

    override fun resolve(
        params: String,
        context: PlaceholderContext,
    ): PlaceholderValue {
        if (params.isEmpty()) return PlaceholderValue.Unknown
        val animation =
            animations()[params]
                ?: return PlaceholderValue.Unavailable(
                    "no animation named '$params' in ${AnimationsConfig.FILE_NAME}",
                )
        val frame = animation.frameAt(Clock.System.now().toEpochMilliseconds())
        if ('%' !in frame) return PlaceholderValue.Text(frame)
        return PlaceholderValue.Text(PlaceholderText.substitute(withoutNestedAnimations(frame), registry(), context))
    }

    /** Blanks any `%basalt_animation_*%` in a frame, so the substitution below cannot re-enter here. */
    private fun withoutNestedAnimations(frame: String): String {
        var result = frame
        for (name in PlaceholderText.namesIn(frame)) {
            if (name.lowercase().startsWith(IDENTIFIER)) result = result.replace("%$name%", "")
        }
        return result
    }

    public companion object {
        public const val IDENTIFIER: String = "basalt_animation"
    }
}
