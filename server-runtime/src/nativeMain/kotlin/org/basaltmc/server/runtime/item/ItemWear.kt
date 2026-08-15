package org.basaltmc.server.runtime.item

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.registry.ItemDurability
import org.basaltmc.server.storage.StoredItemStack
import org.basaltmc.wire.common.Identifier
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtInt

/**
 * How much wear a stack has taken, and what one more use does to it.
 *
 * parity: `ItemStack.isDamageableItem`, `getDamageValue` and `hurtAndBreak`. The damage lives in the
 * `minecraft:damage` component, exactly where vanilla keeps it, so an item that has never been used
 * carries no component at all and one that has carries the number a client draws its bar from.
 */
public object ItemWear {
    public fun isDamageable(stack: StoredItemStack): Boolean {
        if ("minecraft:unbreakable" in stack.components.keys) return false
        return ItemDurability.isDamageable(Identifier.parse(stack.id))
    }

    public fun maxDamage(stack: StoredItemStack): Int = ItemDurability.maxDamageOf(Identifier.parse(stack.id))

    public fun damage(stack: StoredItemStack): Int =
        (stack.components[ItemComponentCodecs.DAMAGE] as? NbtInt)?.value?.coerceIn(0, maxDamage(stack)) ?: 0

    /** What breaking one block costs this stack, or zero for an item that does not wear that way. */
    public fun costPerBlock(stack: StoredItemStack): Int =
        ItemDurability[Identifier.parse(stack.id)]?.damagePerBlock ?: 0

    /** What one attack costs it. Unused until there is combat, and part of the same table. */
    public fun costPerAttack(stack: StoredItemStack): Int =
        ItemDurability[Identifier.parse(stack.id)]?.damagePerAttack ?: 0

    /**
     * [stack] after taking [amount] wear, or null when that broke it.
     *
     * [reduce] is where an enchantment gets to lower the cost (see [ItemDamageEffects]); vanilla
     * applies it before the damage lands, which is why unbreaking makes a tool last longer rather
     * than repairing it afterwards.
     */
    public fun hurt(
        stack: StoredItemStack,
        amount: Int,
        random: BasaltRandom,
        reduce: (StoredItemStack, Int, BasaltRandom) -> Int = { _, value, _ -> value },
    ): StoredItemStack? {
        if (amount <= 0 || !isDamageable(stack)) return stack
        val applied = reduce(stack, amount, random)
        if (applied <= 0) return stack
        val max = maxDamage(stack)
        val newDamage = (damage(stack) + applied).coerceIn(0, max)
        // parity: `ItemStack.isBroken` is `>= max`, so the last point of durability is the one that
        // breaks it rather than leaving a tool sitting at zero.
        if (newDamage >= max) return if (stack.count > 1) stack.copy(count = stack.count - 1) else null
        val components = NbtCompound(stack.components.associate { (key, value) -> key to value })
        components[ItemComponentCodecs.DAMAGE] = NbtInt(newDamage)
        return stack.copy(components = components)
    }
}
