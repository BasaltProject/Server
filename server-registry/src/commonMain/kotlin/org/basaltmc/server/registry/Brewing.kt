package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier

/**
 * One brew: what goes in the bottle, what is dropped on top of it and what comes out.
 *
 * [kind] is `potionMixes` when the ingredient changes the potion inside the bottle, and
 * `containerMixes` when it changes the bottle itself (a potion into a splash potion).
 */
public data class BrewingMix(
    public val kind: String,
    public val from: String,
    public val ingredients: String,
    public val to: String,
)

/**
 * What a brewing stand can make.
 *
 * parity: `PotionBrewing.bootstrap`, which vanilla builds in code rather than in data, so the table
 * is that call's own answer read back out of it.
 */
public object Brewing {
    /** parity: `#minecraft:brewing_fuel`, which is blaze powder and nothing else. */
    public val FUEL: Identifier = Identifier.vanilla("blaze_powder")

    /** parity: `BrewingStandBlockEntity`: twenty brews out of one powder, four hundred ticks each. */
    public const val FUEL_USES: Int = 20
    public const val BREW_TIME: Int = 400

    private val potionMixes = ALL_BREWING_MIXES.filter { it.kind == "potionMixes" }
    private val containerMixes = ALL_BREWING_MIXES.filter { it.kind == "containerMixes" }

    /** Whether [item] is an ingredient any brew uses at all. parity: `PotionBrewing.isIngredient`. */
    public fun isIngredient(item: Identifier): Boolean =
        ALL_BREWING_MIXES.any { item.path in it.ingredients.split(',') }

    /**
     * What [bottle] holding [potion] becomes when [ingredient] is dropped on it, or null when that
     * pair brews nothing. parity: `PotionBrewing.mix`.
     */
    public fun mix(
        bottle: Identifier,
        potion: String?,
        ingredient: Identifier,
    ): Pair<Identifier, String?>? {
        for (mix in containerMixes) {
            if (mix.from != bottle.path) continue
            if (ingredient.path !in mix.ingredients.split(',')) continue
            return Identifier.vanilla(mix.to) to potion
        }
        if (potion == null) return null
        for (mix in potionMixes) {
            if (mix.from != potion) continue
            if (ingredient.path !in mix.ingredients.split(',')) continue
            return bottle to mix.to
        }
        return null
    }

    /** Every mix there is, for tests and for whoever wants to list them. */
    public val all: List<BrewingMix> get() = ALL_BREWING_MIXES
}
