package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.EnderSignal;

public interface EnderSignalOps extends EnderSignal, Cored {
    default org.bukkit.inventory.ItemStack getItem() {
        java.util.Map<String, Object> stored = org.basaltmc.bukkit.item.Nbt.map(EntityNbt.compound(core(), "")
            .get("Item"));
        org.bukkit.inventory.ItemStack stack = stored == null ? null : org.basaltmc.bukkit.item.Convert.stack(stored);
        return stack != null ? stack : new org.bukkit.inventory.ItemStack(org.bukkit.Material.ENDER_EYE);
    }

    default void setItem(org.bukkit.inventory.ItemStack item) {
        if (item == null || item.getType().isAir()) {
            core().nbtRemove("Item");
        } else {
            EntityNbt.put(core(), "Item", org.basaltmc.bukkit.item.Convert.stack(item));
        }
    }

    default int getDespawnTimer() {
        return core().integer("eye_despawn");
    }

    default void setDespawnTimer(int timer) {
        core().set("eye_despawn", timer);
    }

    default boolean getDropItem() {
        return core().flag("eye_drops");
    }

    default void setDropItem(boolean drop) {
        core().set("eye_drops", drop);
    }

    default org.bukkit.Location getTargetLocation() {
        Object stored = core().call("eye_target");
        if (!(stored instanceof String[] position) || position.length != 3) {
            return null;
        }
        return new org.bukkit.Location(getWorld(), Double.parseDouble(position[0]), Double.parseDouble(position[1]),
            Double.parseDouble(position[2]));
    }

    default void setTargetLocation(org.bukkit.Location location) {
        setTargetLocation(location, true);
    }

    default void setTargetLocation(org.bukkit.Location location, boolean update) {
        if (location == null || !java.util.Objects.equals(location.getWorld(), getWorld())) {
            throw new IllegalArgumentException("Cannot target an eye of ender in another world");
        }
        core().call("set_eye_target", location.getX(), location.getY(), location.getZ(), update);
    }
}
