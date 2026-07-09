package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Firework;

public interface FireworkOps extends Firework, Cored {
    default org.bukkit.inventory.ItemStack getItem() {
        java.util.Map<String, Object> stored = org.basaltmc.bukkit.item.Nbt.map(EntityNbt.compound(core(), "")
            .get("FireworksItem"));
        org.bukkit.inventory.ItemStack stack = stored == null ? null : org.basaltmc.bukkit.item.Convert.stack(stored);
        return stack != null ? stack : new org.bukkit.inventory.ItemStack(org.bukkit.Material.FIREWORK_ROCKET);
    }

    default void setItem(org.bukkit.inventory.ItemStack item) {
        if (item == null || item.getType() != org.bukkit.Material.FIREWORK_ROCKET) {
            throw new IllegalArgumentException("A firework holds a firework rocket");
        }
        EntityNbt.put(core(), "FireworksItem", org.basaltmc.bukkit.item.Convert.stack(item));
    }

    default org.bukkit.inventory.meta.FireworkMeta getFireworkMeta() {
        return (org.bukkit.inventory.meta.FireworkMeta) getItem().getItemMeta();
    }

    default void setFireworkMeta(org.bukkit.inventory.meta.FireworkMeta meta) {
        org.bukkit.inventory.ItemStack stack = getItem();
        stack.setItemMeta(meta);
        setItem(stack);
    }

    default int getTicksFlown() {
        return core().nbtInt("Life");
    }

    default void setTicksFlown(int ticks) {
        core().nbtSet("Life", "int", ticks);
    }

    default int getTicksToDetonate() {
        return core().nbtInt("LifeTime");
    }

    default void setTicksToDetonate(int ticks) {
        core().nbtSet("LifeTime", "int", ticks);
    }

    default boolean setLife(int ticks) {
        setTicksFlown(ticks);
        return true;
    }

    default boolean setMaxLife(int ticks) {
        setTicksToDetonate(ticks);
        return true;
    }

    default void detonate() {
        setTicksFlown(getTicksToDetonate() + 1);
    }

    default boolean isShotAtAngle() {
        return core().nbtFlag("ShotAtAngle");
    }

    default void setShotAtAngle(boolean angled) {
        core().nbtSet("ShotAtAngle", "bool", angled);
    }

    default org.bukkit.entity.LivingEntity getAttachedTo() {
        return core().entity(core().integer("attached_to")) instanceof org.bukkit.entity.LivingEntity living ? living
            : null;
    }

    default boolean setAttachedTo(org.bukkit.entity.LivingEntity entity) {
        if (isDetonated()) {
            return false;
        }
        return core().set("attached_to", entity == null ? -1 : entity.getEntityId());
    }

    default boolean isDetonated() {
        return getTicksFlown() > getTicksToDetonate();
    }

    default java.util.UUID getSpawningEntity() {
        return getOwnerUniqueId();
    }
}
