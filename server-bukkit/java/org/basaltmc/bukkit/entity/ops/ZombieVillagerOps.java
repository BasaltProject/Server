package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.ZombieVillager;

public interface ZombieVillagerOps extends ZombieVillager, Cored {
    default void setConversionTime(int time) {
        core().set("conversion_countdown", time);
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

    default org.bukkit.entity.Villager.Type getVillagerType() {
        String name = core().nbtText("VillagerData.type");
        org.bukkit.entity.Villager.Type found = name == null ? null
            : org.bukkit.Registry.VILLAGER_TYPE.get(org.bukkit.NamespacedKey.fromString(name));
        return found == null ? org.bukkit.entity.Villager.Type.PLAINS : found;
    }

    default void setVillagerType(org.bukkit.entity.Villager.Type type) {
        core().nbtSet("VillagerData.type", "string", type.getKey().toString());
    }
}
