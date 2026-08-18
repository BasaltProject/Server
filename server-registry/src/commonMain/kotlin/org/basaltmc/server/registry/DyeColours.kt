package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier

/**
 * The sixteen dyes and the colour each one carries.
 *
 * parity: `DyeColor`'s own `textureDiffuseColor`, which is what `DyedItemColor.applyDyes` averages
 * when leather armour or a firework star is dyed. A hand table rather than a generated one: sixteen
 * numbers that have not moved in a decade, and the generator has no way to name them.
 */
public object DyeColours {
    private val byDye: Map<Identifier, Int> =
        mapOf(
            "white" to 16383998,
            "orange" to 16351261,
            "magenta" to 13061821,
            "light_blue" to 3847130,
            "yellow" to 16701501,
            "lime" to 8439583,
            "pink" to 15961002,
            "gray" to 4673362,
            "light_gray" to 10329495,
            "cyan" to 1481884,
            "purple" to 8991416,
            "blue" to 3949738,
            "brown" to 8606770,
            "green" to 6192150,
            "red" to 11546150,
            "black" to 1908001,
        ).mapKeys { (name, _) -> Identifier.vanilla("${name}_dye") }

    /** The colour [item] dyes with, or null when it is not a dye. */
    public fun of(item: Identifier): Int? = byDye[item]
}
