package org.basaltmc.bukkit.entity.ops;

import org.bukkit.Location;
import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Phantom;

public interface PhantomOps extends Phantom, Cored {
    default int getSize() {
        return core().nbtInt("size");
    }

    default void setSize(int size) {
        core().nbtSet("size", "int", Math.max(0, Math.min(64, size)));
    }

    default Location getAnchorLocation() {
        return EntityNbt.position(core().worldRef(), EntityNbt.compound(core(), "").get("anchor_pos"));
    }

    default void setAnchorLocation(Location location) {
        if (location == null) {
            core().nbtRemove("anchor_pos");
        } else {
            core().nbtSet("anchor_pos", "pos",
                location.getBlockX() + "," + location.getBlockY() + "," + location.getBlockZ());
        }
    }

    default boolean shouldBurnInDay() {
        return core().flag("burns_in_day");
    }

    default void setShouldBurnInDay(boolean burn) {
        core().set("burns_in_day", burn);
    }

    default java.util.UUID getSpawningEntity() {
        Object stored = EntityNbt.compound(core(), "").get("Paper.SpawningEntity");
        return stored instanceof int[] ? org.basaltmc.bukkit.item.Convert.uuidOf(stored) : null;
    }
}
