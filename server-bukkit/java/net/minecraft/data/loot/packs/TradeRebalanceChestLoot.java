// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.loot.packs;
import net.minecraft.data.loot.LootTableSubProvider;
import net.minecraft.world.level.storage.loot.LootTable;
public class TradeRebalanceChestLoot implements LootTableSubProvider {
    public TradeRebalanceChestLoot(LootTableSubProvider.Context output) {
        
    }
    public void run() {
    }
    public LootTable.Builder pillagerOutpostLootTable() {
        return LootTable.lootTable();
    }
    public LootTable.Builder desertPyramidLootTable() {
        return LootTable.lootTable();
    }
    public LootTable.Builder ancientCityLootTable() {
        return LootTable.lootTable();
    }
    public LootTable.Builder jungleTempleLootTable() {
        return LootTable.lootTable();
    }
}
