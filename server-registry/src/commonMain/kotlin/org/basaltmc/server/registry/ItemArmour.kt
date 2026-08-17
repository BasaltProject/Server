package org.basaltmc.server.registry

import org.basaltmc.wire.common.Identifier

/** What one piece of armour is worth, straight from its own attribute modifiers. */
public class ArmourPiece internal constructor(
    public val slot: String,
    public val armour: Float,
    public val toughness: Float,
    public val knockbackResistance: Float,
)

/** parity: the `minecraft:attribute_modifiers` of every wearable item. */
public object ItemArmour {
    private val byItem: Map<Identifier, ArmourPiece> =
        ALL_ITEM_ARMOUR
            .associate { encoded ->
                val fields = encoded.split('|')
                Identifier.vanilla(fields[0]) to
                    ArmourPiece(fields[1], fields[2].toFloat(), fields[3].toFloat(), fields[4].toFloat())
            }

    public fun of(item: Identifier): ArmourPiece? = byItem[item]
}
