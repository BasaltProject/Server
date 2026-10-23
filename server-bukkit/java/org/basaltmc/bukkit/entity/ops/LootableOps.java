package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.NamespacedKey;
import org.bukkit.loot.LootTable;
import org.bukkit.loot.Lootable;

public interface LootableOps extends Lootable, Cored {
    default LootTable getLootTable() {
        String table = core().text("loot_table");
        return table == null ? null : org.bukkit.Bukkit.getLootTable(NamespacedKey.fromString(table));
    }

    default void setLootTable(LootTable table) {
        core().set("loot_table", table == null ? null : table.getKey().toString());
    }

    default long getSeed() {
        return core().property("loot_seed") instanceof Number seed ? seed.longValue() : 0L;
    }

    default void setSeed(long seed) {
        core().set("loot_seed", seed);
    }
}
