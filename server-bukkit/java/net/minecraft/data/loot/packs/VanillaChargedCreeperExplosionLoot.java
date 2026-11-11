// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.loot.packs;
import net.minecraft.data.loot.LootTableSubProvider;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.item.Item;
import net.minecraft.world.level.storage.loot.LootTable;
public class VanillaChargedCreeperExplosionLoot implements LootTableSubProvider {
    public VanillaChargedCreeperExplosionLoot(LootTableSubProvider.Context output) {
        
    }
    public void run() {
    }
    static record Entry(ResourceKey<LootTable> lootTable, EntityType<?> entityType, Item item) {
    }
}
