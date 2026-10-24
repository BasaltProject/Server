package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.ThrownPotion;

public interface ThrownPotionOps extends ThrownPotion, Cored {
    default org.bukkit.inventory.meta.PotionMeta getPotionMeta() {
        return (org.bukkit.inventory.meta.PotionMeta) getItem().getItemMeta();
    }

    default void setPotionMeta(org.bukkit.inventory.meta.PotionMeta meta) {
        org.bukkit.inventory.ItemStack stack = getItem();
        stack.setItemMeta(meta);
        setItem(stack);
    }

    default java.util.Collection<org.bukkit.potion.PotionEffect> getEffects() {
        org.bukkit.inventory.meta.PotionMeta meta = getPotionMeta();
        java.util.List<org.bukkit.potion.PotionEffect> effects = new java.util.ArrayList<>();
        if (meta.getBasePotionType() != null) {
            effects.addAll(meta.getBasePotionType().getPotionEffects());
        }
        effects.addAll(meta.getCustomEffects());
        return effects;
    }
}
