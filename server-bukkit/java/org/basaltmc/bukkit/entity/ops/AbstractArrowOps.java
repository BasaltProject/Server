package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.AbstractArrow;

public interface AbstractArrowOps extends AbstractArrow, Cored {
    default boolean isCritical() {
        return core().flag("critical");
    }

    default void setCritical(boolean critical) {
        core().set("critical", critical);
    }

    default AbstractArrow.PickupStatus getPickupStatus() {
        return AbstractArrow.PickupStatus.values()[Math.min(2, Math.max(0, core().integer("arrow_pickup")))];
    }

    default void setPickupStatus(AbstractArrow.PickupStatus status) {
        core().set("arrow_pickup", status.ordinal());
    }

    default int getPierceLevel() {
        return core().integer("pierce_level");
    }

    default void setPierceLevel(int level) {
        core().set("pierce_level", level);
    }

    default boolean isInBlock() {
        return isOnGround();
    }

    default double getDamage() {
        return core().nbtValue("damage") instanceof Number damage ? damage.doubleValue() : 2.0;
    }

    default void setDamage(double damage) {
        if (damage < 0) {
            throw new IllegalArgumentException("Damage must be positive");
        }
        core().nbtSet("damage", "double", damage);
    }

    default org.bukkit.Sound getHitSound() {
        String id = core().nbtText("SoundEvent");
        return org.bukkit.Registry.SOUNDS.get(org.bukkit.NamespacedKey.fromString(id == null
            ? "minecraft:entity.arrow.hit" : id));
    }

    default void setHitSound(org.bukkit.Sound sound) {
        core().nbtSet("SoundEvent", "string", sound.getKey().toString());
    }

    default org.bukkit.inventory.ItemStack getItem() {
        return EntityNbt.item(core(), "item", org.bukkit.Material.ARROW);
    }

    default void setItem(org.bukkit.inventory.ItemStack item) {
        EntityNbt.storeItem(core(), "item", item);
    }

    default org.bukkit.inventory.ItemStack getItemStack() {
        return getItem();
    }

    default void setItemStack(org.bukkit.inventory.ItemStack item) {
        setItem(item);
    }

    default void setWeapon(org.bukkit.inventory.ItemStack weapon) {
        EntityNbt.storeItem(core(), "weapon", weapon);
    }

    default boolean isShotFromCrossbow() {
        if ("minecraft:crossbow".equals(core().text("fired_from"))) {
            return true;
        }
        org.bukkit.inventory.ItemStack weapon = getWeapon();
        return weapon != null && weapon.getType() == org.bukkit.Material.CROSSBOW;
    }

    default org.bukkit.block.Block getAttachedBlock() {
        return isInBlock() ? getLocation().getBlock() : null;
    }

    default java.util.List<org.bukkit.block.Block> getAttachedBlocks() {
        org.bukkit.block.Block block = getAttachedBlock();
        return block == null ? java.util.List.of() : java.util.List.of(block);
    }

    default void setShooter(org.bukkit.projectiles.ProjectileSource source, boolean resetLeft) {
        setShooter(source);
    }

    default int getKnockbackStrength() {
        return core().integer("arrow_knockback");
    }

    default void setKnockbackStrength(int strength) {
        if (strength < 0) {
            throw new IllegalArgumentException("Knockback strength cannot be negative");
        }
        core().set("arrow_knockback", strength);
    }

    default int getLifetimeTicks() {
        return core().integer("arrow_life");
    }

    default void setLifetimeTicks(int ticks) {
        core().set("arrow_life", ticks);
    }

    default void setShotFromCrossbow(boolean crossbow) {
        if (crossbow) {
            core().set("fired_from", "minecraft:crossbow");
        } else if ("minecraft:crossbow".equals(core().text("fired_from"))) {
            core().set("fired_from", "minecraft:bow");
        }
    }
}
