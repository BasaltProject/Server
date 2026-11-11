// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.loot.packs;
import net.minecraft.data.loot.LootTableSubProvider;
import net.minecraft.world.level.storage.loot.LootTable;
public class VanillaFishingLoot implements LootTableSubProvider {
    public VanillaFishingLoot(LootTableSubProvider.Context output) {
        
    }
    public void run() {
    }
    public static LootTable.Builder fishingFishLootTable() {
        return LootTable.lootTable();
    }
}
