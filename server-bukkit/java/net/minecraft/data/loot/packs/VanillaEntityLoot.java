// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.loot.packs;
import net.minecraft.data.loot.EntityLootSubProvider;
import net.minecraft.data.loot.LootTableSubProvider;
import net.minecraft.world.level.storage.loot.LootTable;
public class VanillaEntityLoot extends EntityLootSubProvider {
    public VanillaEntityLoot(LootTableSubProvider.Context output) {
        super((net.minecraft.world.flag.FeatureFlagSet) null, (net.minecraft.data.loot.LootTableSubProvider.Context) null);
    }
    public void generate() {
    }
    public LootTable.Builder elderGuardianLootTable() {
        return LootTable.lootTable();
    }
}
