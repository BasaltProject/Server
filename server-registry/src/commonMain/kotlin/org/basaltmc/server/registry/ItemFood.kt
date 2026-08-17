package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier

/** What eating something does to whoever ate it. parity: `Consumable.onConsumeEffects`. */
public sealed interface ConsumeEffect {
    public class Apply(
        public val effect: Identifier,
        public val durationTicks: Int,
        public val amplifier: Int,
        public val probability: Float,
    ) : ConsumeEffect

    public class Remove(
        public val effect: Identifier,
    ) : ConsumeEffect

    public object ClearAll : ConsumeEffect
}

/** What eating one of these is worth. parity: the `minecraft:food` and `minecraft:consumable` components. */
public class FoodValue internal constructor(
    public val nutrition: Int,
    public val saturation: Float,
    public val canAlwaysEat: Boolean,
    public val consumeSeconds: Float,
    public val effects: List<ConsumeEffect>,
    /** parity: `minecraft:use_remainder`: the bowl, the bottle or the bucket left in hand. */
    public val remainder: Identifier?,
) {
    /** parity: `Consumable.getConsumeTicks`, which is the seconds at twenty ticks each. */
    public val consumeTicks: Int get() = (consumeSeconds * TICKS_PER_SECOND).toInt()

    private companion object {
        const val TICKS_PER_SECOND = 20
    }
}

public object ItemFood {
    private val byItem: Map<Identifier, FoodValue> =
        ALL_ITEM_FOOD.associate { encoded ->
            val fields = encoded.split('|')
            Identifier.vanilla(fields[0]) to
                FoodValue(
                    nutrition = fields[1].toInt(),
                    saturation = fields[2].toFloat(),
                    canAlwaysEat = fields[3].toBoolean(),
                    consumeSeconds = fields[4].toFloat(),
                    effects = parseEffects(fields[5]),
                    remainder = fields[6].takeIf { it.isNotEmpty() }?.let(Identifier::vanilla),
                )
        }

    public fun of(item: Identifier): FoodValue? = byItem[item]

    private fun parseEffects(encoded: String): List<ConsumeEffect> =
        encoded
            .split(';')
            .filter { it.isNotEmpty() }
            .mapNotNull { entry ->
                val body = entry.substringAfter(':', "")
                when {
                    entry == "clear" -> ConsumeEffect.ClearAll
                    entry.startsWith("remove:") -> ConsumeEffect.Remove(Identifier.vanilla(body))
                    entry.startsWith("apply:") -> {
                        val parts = body.split(',')
                        ConsumeEffect.Apply(
                            effect = Identifier.vanilla(parts[0]),
                            durationTicks = parts[1].toInt(),
                            amplifier = parts[2].toInt(),
                            probability = parts[3].toFloat(),
                        )
                    }
                    else -> null
                }
            }
}
