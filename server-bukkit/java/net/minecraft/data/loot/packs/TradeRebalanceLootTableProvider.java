// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.loot.packs;
import net.minecraft.core.registries.SingleRegistryBootstrap;
import net.minecraft.world.level.storage.loot.LootTable;
public class TradeRebalanceLootTableProvider {
    public static SingleRegistryBootstrap<LootTable> create() {
        return new net.minecraft.data.loot.LootTableProvider(java.util.Set.of(), java.util.List.of());
    }
}
