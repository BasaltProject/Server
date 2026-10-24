package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.ZombieVillager;

public interface ZombieVillagerOps extends ZombieVillager, Cored {
    default void setConversionTime(int time) {
        core().nbtSet("ConversionTime", "int", time);
    }

    default void setConversionTime(int time, boolean broadcast) {
        setConversionTime(time);
    }

    default org.bukkit.OfflinePlayer getConversionPlayer() {
        Object stored = EntityNbt.compound(core(), "").get("ConversionPlayer");
        return stored instanceof int[] ? org.bukkit.Bukkit.getOfflinePlayer(org.basaltmc.bukkit.item.Convert.uuidOf(stored))
            : null;
    }

    default void setConversionPlayer(org.bukkit.OfflinePlayer player) {
        if (player == null) {
            core().nbtRemove("ConversionPlayer");
        } else {
            core().nbtSet("ConversionPlayer", "uuid", player.getUniqueId().toString());
        }
    }
}
