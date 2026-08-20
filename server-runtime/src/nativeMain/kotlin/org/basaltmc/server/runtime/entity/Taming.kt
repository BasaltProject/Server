package org.basaltmc.server.runtime.entity

import org.basaltmc.wire.common.Identifier

/**
 * The mobs a player can make their own.
 *
 * parity: `Wolf.mobInteract`, `Cat.mobInteract` and `Ocelot.mobInteract`: the right food in hand,
 * a roll for whether it takes, and from then on the animal has an owner, can be told to sit, and
 * fights whoever fights them.
 */
public object Taming {
    /** parity: what each one is tamed with. */
    private val TAMING_FOOD: Map<String, Set<String>> =
        mapOf(
            "wolf" to setOf("bone"),
            "cat" to setOf("cod", "salmon"),
            "ocelot" to setOf("cod", "salmon"),
            "parrot" to setOf("wheat_seeds", "melon_seeds", "pumpkin_seeds", "beetroot_seeds", "torchflower_seeds"),
        )

    /** parity: `Wolf.tame`'s own one in three, which is why a wolf takes a handful of bones. */
    public const val TAME_ODDS: Int = 3

    /** parity: `TamableAnimal`'s own byte, whose bits the client reads to draw a sitting pet. */
    public const val FLAGS_INDEX: Int = 17
    public const val FLAG_SITTING: Int = 0x01
    public const val FLAG_TAMED: Int = 0x04

    /** parity: `EntityEvent.TAMING_FAILED` and `TAMING_SUCCEEDED`, the smoke and the hearts. */
    public const val EVENT_TAMING_FAILED: Int = 6
    public const val EVENT_TAMING_SUCCEEDED: Int = 7

    public fun isTamable(type: Identifier): Boolean = type.path in TAMING_FOOD

    public fun tamedBy(
        type: Identifier,
        item: Identifier,
    ): Boolean = TAMING_FOOD[type.path]?.contains(item.path) == true

    /** The flags byte for this animal, as the client wants it. */
    public fun flagsOf(entity: ServerEntity): Int {
        var flags = 0
        if (entity.sitting) flags = flags or FLAG_SITTING
        if (entity.owner != null) flags = flags or FLAG_TAMED
        return flags
    }
}
