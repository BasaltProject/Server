package org.basaltmc.server.world.crafting

import org.basaltmc.server.world.loot.ItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt
import org.basaltmc.wire.nbt.NbtTag

/**
 * The recipes whose result is a function of what went in.
 *
 * parity: the `crafting_special_*` family, `crafting_dye` and `crafting_imbue`. 26.2 keeps their
 * ingredients in the recipe file, so what is code here is the rule and what it writes onto the
 * components.
 */
public object SpecialRecipes {
    /** parity: `RepairItemRecipe`, the extra five per cent of the bar a repair gives. */
    private const val REPAIR_BONUS_PERCENT = 5

    /** How much damage this stack has taken, from its own components. */
    public fun damageOf(stack: ItemStack): Int = (stack.components[DAMAGE] as? NbtInt)?.value ?: 0

    public const val DAMAGE: String = "minecraft:damage"
    public const val MAX_DAMAGE: String = "minecraft:max_damage"
    public const val DYED_COLOR: String = "minecraft:dyed_color"
    public const val BANNER_PATTERNS: String = "minecraft:banner_patterns"
    public const val WRITTEN_BOOK_CONTENT: String = "minecraft:written_book_content"
    public const val MAP_ID: String = "minecraft:map_id"
    public const val POTION_CONTENTS: String = "minecraft:potion_contents"

    /** parity: `DataComponents.TRIM`, the pattern and material a piece of armour wears. */
    public const val TRIM: String = "minecraft:trim"

    /**
     * The trim material an item provides, filled in by the caller.
     *
     * parity: `DataComponents.PROVIDES_TRIM_MATERIAL`, which is a component on the ingot or gem
     * rather than a list anywhere: an iron ingot provides `minecraft:iron`, and an item that
     * provides nothing is not a trim material at all.
     */
    public var trimMaterialOf: (Identifier) -> Identifier? = { null }

    /**
     * parity: `RepairItemRecipe.assemble`: the two bars added together plus five per cent of the
     * item's own, capped at full.
     */
    public fun repair(
        first: ItemStack,
        second: ItemStack,
        maxDamageOf: (Identifier) -> Int,
    ): ItemStack? {
        if (first.item != second.item) return null
        val durability = maxDamageOf(first.item)
        if (durability <= 0) return null
        val remaining =
            (durability - damageOf(first)) + (durability - damageOf(second)) +
                durability * REPAIR_BONUS_PERCENT / 100
        val components = NbtCompound()
        components[DAMAGE] = NbtInt(maxOf(durability - remaining, 0))
        return ItemStack(first.item, 1, components)
    }

    /** parity: `DyedItemColor.applyDyes`, the running average every dye pulls the colour towards. */
    public fun mixDyes(
        current: Int?,
        dyes: List<Int>,
    ): Int {
        if (dyes.isEmpty()) return current ?: 0
        var red = 0
        var green = 0
        var blue = 0
        var total = 0
        var count = 0
        if (current != null) {
            val r = (current shr 16) and 0xFF
            val g = (current shr 8) and 0xFF
            val b = current and 0xFF
            total += maxOf(r, maxOf(g, b))
            red += r
            green += g
            blue += b
            count++
        }
        for (dye in dyes) {
            val r = (dye shr 16) and 0xFF
            val g = (dye shr 8) and 0xFF
            val b = dye and 0xFF
            total += maxOf(r, maxOf(g, b))
            red += r
            green += g
            blue += b
            count++
        }
        var averageRed = red / count
        var averageGreen = green / count
        var averageBlue = blue / count
        val averageTotal = total.toFloat() / count
        val brightest = maxOf(averageRed, maxOf(averageGreen, averageBlue)).toFloat()
        if (brightest > 0f) {
            val scale = averageTotal / brightest
            averageRed = (averageRed * scale).toInt()
            averageGreen = (averageGreen * scale).toInt()
            averageBlue = (averageBlue * scale).toInt()
        }
        return (averageRed shl 16) or (averageGreen shl 8) or averageBlue
    }

    /** Copies [from]'s components onto a new stack of [item]. parity: `createWithOriginalComponents`. */
    public fun withComponentsOf(
        item: Identifier,
        count: Int,
        from: ItemStack,
        vararg overrides: Pair<String, NbtTag>,
    ): ItemStack {
        val components = NbtCompound()
        for (key in from.components.keys) from.components[key]?.let { components[key] = it }
        for ((key, value) in overrides) components[key] = value
        return ItemStack(item, count, components)
    }
}
