package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Trident;

public interface TridentOps extends Trident, Cored {
    default boolean hasDealtDamage() {
        return core().nbtFlag("DealtDamage");
    }

    default void setHasDealtDamage(boolean dealt) {
        core().nbtSet("DealtDamage", "bool", dealt);
    }

    default int getLoyaltyLevel() {
        return getItemStack().getEnchantmentLevel(org.bukkit.enchantments.Enchantment.LOYALTY);
    }

    default void setLoyaltyLevel(int level) {
        org.bukkit.inventory.ItemStack stack = getItemStack();
        if (level <= 0) {
            stack.removeEnchantment(org.bukkit.enchantments.Enchantment.LOYALTY);
        } else {
            stack.addUnsafeEnchantment(org.bukkit.enchantments.Enchantment.LOYALTY, level);
        }
        setItemStack(stack);
    }

    default boolean hasGlint() {
        return core().flag("trident_glint");
    }

    default void setGlint(boolean glint) {
        core().set("trident_glint", glint);
    }
}
