package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Endermite;

public interface EndermiteOps extends Endermite, Cored {
    org.bukkit.NamespacedKey kPlayerSpawned = new org.bukkit.NamespacedKey("basalt", "player_spawned");

    default int getLifetimeTicks() {
        return core().nbtInt("Lifetime");
    }

    default void setLifetimeTicks(int ticks) {
        core().nbtSet("Lifetime", "int", ticks);
    }

    default boolean isPlayerSpawned() {
        return core().persistentData().has(kPlayerSpawned, org.bukkit.persistence.PersistentDataType.BOOLEAN);
    }

    default void setPlayerSpawned(boolean playerSpawned) {
        if (playerSpawned) {
            core().persistentData().set(kPlayerSpawned, org.bukkit.persistence.PersistentDataType.BOOLEAN, true);
        } else {
            core().persistentData().remove(kPlayerSpawned);
        }
    }
}
