package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;

public interface WitchOps extends org.bukkit.entity.Witch, Cored {
    default boolean isDrinkingPotion() {
        return core().flag("witch_drinking");
    }

    default int getPotionUseTimeLeft() {
        return core().integer("witch_use_ticks");
    }

    default void setPotionUseTimeLeft(int ticks) {
        core().set("witch_use_ticks", ticks);
    }

    default org.bukkit.inventory.ItemStack getDrinkingPotion() {
        return getEquipment().getItemInMainHand();
    }

    default void setDrinkingPotion(org.bukkit.inventory.ItemStack potion) {
        boolean empty = potion == null || potion.getType().isAir();
        if (!empty && potion.getType() != org.bukkit.Material.POTION) {
            throw new IllegalArgumentException("must be potion, air, or null");
        }
        core().call("witch_drink", empty ? null : org.basaltmc.bukkit.item.Nbt.writeNamed(
            org.basaltmc.bukkit.item.Convert.stack(potion)));
    }
}
