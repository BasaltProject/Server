package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.OminousItemSpawner;

public interface OminousItemSpawnerOps extends OminousItemSpawner, Cored {
    default org.bukkit.inventory.ItemStack getItem() {
        return EntityNbt.item(core(), "item", org.bukkit.Material.AIR);
    }

    default void setItem(org.bukkit.inventory.ItemStack item) {
        EntityNbt.storeItem(core(), "item", item);
    }

    default long getSpawnItemAfterTicks() {
        return core().nbtValue("spawn_item_after_ticks") instanceof Number ticks ? ticks.longValue() : 0L;
    }

    default void setSpawnItemAfterTicks(long ticks) {
        core().nbtSet("spawn_item_after_ticks", "long", ticks);
    }
}
