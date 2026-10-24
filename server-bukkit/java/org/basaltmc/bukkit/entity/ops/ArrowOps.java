package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Arrow;

public interface ArrowOps extends Arrow, Cored {
    private EntityPotions potion() {
        return new EntityPotions(core(), "potion_contents");
    }

    default void setBasePotionData(org.bukkit.potion.PotionData data) {
        potion().type(EntityPotions.of(data));
    }

    default org.bukkit.potion.PotionData getBasePotionData() {
        return EntityPotions.dataOf(potion().type());
    }

    default void setBasePotionType(org.bukkit.potion.PotionType type) {
        potion().type(type);
    }

    default org.bukkit.potion.PotionType getBasePotionType() {
        return potion().type();
    }

    default org.bukkit.Color getColor() {
        return potion().color();
    }

    default void setColor(org.bukkit.Color color) {
        potion().color(color);
    }

    default boolean hasCustomEffects() {
        return !potion().effects().isEmpty();
    }

    default java.util.List<org.bukkit.potion.PotionEffect> getCustomEffects() {
        return potion().effects();
    }

    default boolean addCustomEffect(org.bukkit.potion.PotionEffect effect, boolean override) {
        return potion().add(effect, override);
    }

    default boolean removeCustomEffect(org.bukkit.potion.PotionEffectType type) {
        return potion().remove(type);
    }

    default boolean hasCustomEffect(org.bukkit.potion.PotionEffectType type) {
        return potion().has(type);
    }

    default void clearCustomEffects() {
        potion().clear();
    }
}
