package org.basaltmc.server.runtime.item

import org.basaltmc.server.registry.DataComponents
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtByte
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtList
import org.basaltmc.wire.nbt.NbtString
import org.basaltmc.wire.nbt.NbtTag
import org.basaltmc.wire.nbt.NbtType
import org.basaltmc.wire.protocol.play.ItemComponent
import org.basaltmc.wire.text.ComponentNbt

/**
 * The two forms of an item component, and the translation between them: the persistent one `/give`
 * writes and `players.db` keeps, and the network one a client reads.
 *
 * Enchantments are the only ones needing the registry, since they travel as ids, so both directions
 * take a resolver that may answer null.
 */
public object ItemComponentCodecs {
    /** `minecraft:enchantments`, where an enchanted stack keeps its levels by name. */
    public const val ENCHANTMENTS: String = "minecraft:enchantments"
    public const val STORED_ENCHANTMENTS: String = "minecraft:stored_enchantments"
    public const val DAMAGE: String = "minecraft:damage"

    private val NUMBERS =
        mapOf(
            "minecraft:max_stack_size" to ItemComponent.MAX_STACK_SIZE,
            "minecraft:max_damage" to ItemComponent.MAX_DAMAGE,
            DAMAGE to ItemComponent.DAMAGE,
            "minecraft:repair_cost" to ItemComponent.REPAIR_COST,
        )

    private val UNITS =
        mapOf(
            "minecraft:unbreakable" to ItemComponent.UNBREAKABLE,
            "minecraft:creative_slot_lock" to ItemComponent.CREATIVE_SLOT_LOCK,
        )

    private val TEXTS =
        mapOf(
            "minecraft:custom_name" to ItemComponent.CUSTOM_NAME,
            "minecraft:item_name" to ItemComponent.ITEM_NAME,
        )

    /** parity: `Rarity`'s own order, which is what its `idMapper` stream codec writes. */
    private val RARITIES = listOf("common", "uncommon", "rare", "epic")

    /** The components with a network codec of their own here. See also [ItemComponentPayloads]. */
    public val typed: Set<String> =
        NUMBERS.keys + UNITS.keys + TEXTS.keys +
            setOf(
                ENCHANTMENTS,
                STORED_ENCHANTMENTS,
                "minecraft:custom_data",
                "minecraft:lore",
                "minecraft:rarity",
                "minecraft:dyed_color",
                "minecraft:enchantment_glint_override",
            )

    /**
     * Every component a command may set, which is everything the registry knows.
     *
     * A component with no network codec on either side is still stored and persisted, which is why
     * the command takes it; what it cannot do is reach the client, and [notDrawn] says which.
     */
    public fun exists(name: String): Boolean = DataComponents[Identifier.parse(name)] != null

    /** Whether a stack carrying [name] looks right on a client, or loses it on the way out. */
    public fun isSent(name: String): Boolean = name in typed || name in ItemComponentPayloads.supported

    /** The components of [components] no client will see, by name and in the order they were written. */
    public fun notDrawn(components: NbtCompound): List<String> = components.keys.filterNot(::isSent)

    /**
     * One stored component in the form the wire wants, or null when its value does not fit the
     * component (a `damage` that is not a number, an enchantment nobody can resolve).
     */
    public fun toWire(
        name: String,
        value: NbtTag,
        enchantmentId: (Identifier) -> Int?,
    ): ItemComponent? {
        NUMBERS[name]?.let { id -> return intOf(value)?.let { ItemComponent.Number(id, it) } }
        UNITS[name]?.let { return ItemComponent.Unit(it) }
        TEXTS[name]?.let { id -> return ItemComponent.Text(id, ComponentNbt.deserialize(value)) }
        return when (name) {
            ENCHANTMENTS, STORED_ENCHANTMENTS -> {
                val compound = value as? NbtCompound ?: return null
                val levels =
                    buildMap {
                        for (key in compound.keys) {
                            val id = enchantmentId(Identifier.parse(key)) ?: continue
                            put(id, compound.getInt(key))
                        }
                    }
                if (levels.isEmpty()) null else ItemComponent.Enchantments(levels, name == STORED_ENCHANTMENTS)
            }
            "minecraft:custom_data" -> (value as? NbtCompound)?.let { ItemComponent.CustomData(it) }
            "minecraft:lore" ->
                (value as? NbtList)?.let { list ->
                    ItemComponent.Lore((0 until list.size).map { ComponentNbt.deserialize(list[it]) })
                }
            "minecraft:rarity" ->
                RARITIES
                    .indexOf((value as? NbtString)?.value)
                    .takeIf { it >= 0 }
                    ?.let { ItemComponent.Number(ItemComponent.RARITY, it) }
            "minecraft:dyed_color" -> intOf(value)?.let { ItemComponent.Color(it) }
            "minecraft:enchantment_glint_override" ->
                intOf(value)?.let {
                    ItemComponent.Flag(
                        ItemComponent.ENCHANTMENT_GLINT_OVERRIDE,
                        it != 0,
                    )
                }
            // Everything else with a network form of its own is written straight into bytes.
            else -> ItemComponentPayloads.encode(name, value)
        }
    }

    /**
     * One component off the wire in the form this server stores, or null for one it does not model:
     * a client can only echo back what it was sent, so dropping those loses nothing it was given.
     */
    public fun toStored(
        component: ItemComponent,
        enchantmentName: (Int) -> Identifier?,
    ): Pair<String, NbtTag>? {
        val name = DataComponents.nameOf(component.id)?.toString() ?: return null
        return when (component) {
            is ItemComponent.Enchantments -> {
                val stored = NbtCompound()
                for ((id, level) in component.levels) {
                    val enchantment = enchantmentName(id) ?: continue
                    stored.putInt(enchantment.toString(), level)
                }
                if (stored.keys.isEmpty()) null else name to stored
            }
            is ItemComponent.Number ->
                if (component.id == ItemComponent.RARITY) {
                    RARITIES.getOrNull(component.value)?.let { name to NbtString(it) }
                } else {
                    name to NbtInt(component.value)
                }
            is ItemComponent.Color -> name to NbtInt(component.rgb)
            is ItemComponent.Flag -> name to NbtByte(if (component.value) 1 else 0)
            is ItemComponent.Unit -> name to NbtCompound()
            is ItemComponent.Text -> name to ComponentNbt.serialize(component.text)
            is ItemComponent.Lore ->
                name to NbtList(NbtType.COMPOUND, component.lines.map(ComponentNbt::serialize))
            is ItemComponent.CustomData -> name to component.tag
            is ItemComponent.Raw -> null
        }
    }

    /** Every stored component of a stack in wire order, skipping the ones that do not resolve. */
    public fun toWire(
        components: NbtCompound,
        enchantmentId: (Identifier) -> Int?,
    ): List<ItemComponent> =
        components.keys.mapNotNull { key ->
            components[key]?.let { toWire(key, it, enchantmentId) }
        }

    /** The reverse, for a stack a client sent back. */
    public fun toStored(
        components: List<ItemComponent>,
        enchantmentName: (Int) -> Identifier?,
    ): NbtCompound {
        val stored = NbtCompound()
        for (component in components) {
            val (name, value) = toStored(component, enchantmentName) ?: continue
            stored[name] = value
        }
        return stored
    }

    private fun intOf(value: NbtTag): Int? =
        when (value) {
            is NbtInt -> value.value
            is NbtByte -> value.value.toInt()
            else -> null
        }
}
