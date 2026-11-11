// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.loot;
import java.util.List;
import java.util.Set;
import net.minecraft.core.registries.SingleRegistryBootstrap;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
import net.minecraft.util.context.ContextKeySet;
import net.minecraft.world.level.storage.loot.LootTable;
public class LootTableProvider implements SingleRegistryBootstrap<LootTable> {
    public LootTableProvider(Set<ResourceKey<LootTable>> requiredTables, List<SubProviderEntry> subProviders) {
        
    }
    public void run(BootstrapContext<LootTable> context) {
    }
    public static record SubProviderEntry(LootTableSubProvider.Factory bootstrap, ContextKeySet paramSet) {
    }
}
