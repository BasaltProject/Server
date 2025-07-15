package org.basaltmc.server.runtime.world

import org.basaltmc.server.registry.AttributeIds
import org.basaltmc.server.runtime.player.PlayerInventory
import org.basaltmc.server.storage.StoredItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.protocol.play.UpdateAttributesPacket

/**
 * The attribute modifiers an enchanted item gives whoever holds it.
 *
 * The client reads mining speed off its own attribute map, which only the server fills: efficiency
 * felt like nothing at all because nobody ever sent it. parity: `EnchantmentHelper.forEachModifier`
 * plus `LivingEntity`, which adds the modifiers of what is equipped and syncs the attribute.
 */
public object EnchantmentAttributes {
    /**
     * parity: the `minecraft:attribute` registry ids the enchantments of a held item can touch.
     *
     * The numbers come from the registry rather than being written out here: one of them was wrong
     * (`attack_speed` was going out as `attack_knockback`), which is the mistake a hand-copied table
     * makes and nobody spots by reading.
     */
    private val PROTOCOL_IDS: Map<String, Int> =
        listOf(
            "minecraft:mining_efficiency",
            "minecraft:block_break_speed",
            "minecraft:attack_damage",
            "minecraft:attack_speed",
            "minecraft:submerged_mining_speed",
            "minecraft:sweeping_damage_ratio",
        ).associateWith { requireNotNull(AttributeIds.protocolId(it)) { "no attribute called '$it'" } }

    /** Every attribute a held stack modifies, as the snapshots the update packet carries. */
    public fun snapshotsFor(
        stack: StoredItemStack?,
        definitionOf: (Identifier) -> NbtCompound?,
    ): List<UpdateAttributesPacket.AttributeSnapshot> {
        val modifiers = mutableMapOf<String, MutableList<UpdateAttributesPacket.Modifier>>()
        if (stack != null) {
            for ((id, level) in PlayerInventory.enchantmentLevels(stack)) {
                val definition = definitionOf(id) ?: continue
                if (!appliesToMainHand(definition)) continue
                for (effect in attributeEffects(definition)) {
                    val attribute = effect.getString("attribute").ifEmpty { continue }
                    if (attribute !in PROTOCOL_IDS) continue
                    modifiers
                        .getOrPut(attribute) { mutableListOf() }
                        .add(
                            UpdateAttributesPacket.Modifier(
                                id = Identifier.parse(effect.getString("id").ifEmpty { "minecraft:enchantment.$id" }),
                                amount = amountOf(effect["amount"] as? NbtCompound, level),
                                operation = operationOf(effect.getString("operation")),
                            ),
                        )
                }
            }
        }
        // Every attribute this build can modify is sent every time, so one that stops applying goes
        // back to its base rather than keeping the last modifier the client was told about.
        return PROTOCOL_IDS.map { (name, protocolId) ->
            UpdateAttributesPacket.AttributeSnapshot(
                attribute = protocolId,
                base = baseOf(name),
                modifiers = modifiers[name].orEmpty(),
            )
        }
    }

    /** parity: `Enchantment.definition().slots()`, of which only the main hand is held. */
    private fun appliesToMainHand(definition: NbtCompound): Boolean {
        val slots = definition["slots"] as? NbtList ?: return false
        return slots.any { (it as? org.basaltmc.wire.nbt.NbtString)?.value in MAIN_HAND_SLOTS }
    }

    private fun attributeEffects(definition: NbtCompound): List<NbtCompound> {
        val effects = definition["effects"] as? NbtCompound ?: return emptyList()
        val attributes = effects["minecraft:attributes"] as? NbtList ?: return emptyList()
        return attributes.filterIsInstance<NbtCompound>()
    }

    /** parity: `LevelBasedValue`, of which enchantments use these three shapes. */
    private fun amountOf(
        amount: NbtCompound?,
        level: Int,
    ): Double {
        if (amount == null) return 0.0
        val base = amount.getDouble("base")
        return when (amount.getString("type")) {
            "minecraft:levels_squared" -> (level * level + amount.getDouble("added"))
            "minecraft:linear" -> base + amount.getDouble("per_level_above_first") * (level - 1)
            else -> amount.getDouble("value").takeIf { it != 0.0 } ?: base
        }
    }

    private fun operationOf(name: String): UpdateAttributesPacket.Operation =
        when (name) {
            "add_multiplied_base" -> UpdateAttributesPacket.Operation.ADD_MULTIPLIED_BASE
            "add_multiplied_total" -> UpdateAttributesPacket.Operation.ADD_MULTIPLIED_TOTAL
            else -> UpdateAttributesPacket.Operation.ADD_VALUE
        }

    /** parity: `Attributes`' own defaults for the ones a held item touches. */
    private fun baseOf(attribute: String): Double =
        when (attribute) {
            "minecraft:block_break_speed", "minecraft:attack_speed" -> 1.0
            "minecraft:submerged_mining_speed" -> 0.2
            "minecraft:attack_damage" -> 1.0
            else -> 0.0
        }

    private val MAIN_HAND_SLOTS = setOf("mainhand", "hand", "any")
}
