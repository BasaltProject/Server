// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server;
import java.util.List;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Executor;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.LayeredRegistryAccess;
import net.minecraft.core.Registry;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.packs.resources.ResourceManager;
import net.minecraft.world.level.storage.loot.LootTable;
public class ReloadableServerRegistries {
    public static CompletableFuture<LoadResult> reload(LayeredRegistryAccess<RegistryLayer> context, List<Registry.PendingTags<?>> updatedContextTags, ResourceManager manager, Executor executor) {
        return CompletableFuture.completedFuture(new LoadResult(context, MinecraftServer.getServer().registryAccess()));
    }
    public static record LoadResult(LayeredRegistryAccess<RegistryLayer> layers, HolderLookup.Provider lookupWithUpdatedTags) {
    }
    public static class Holder {
        private final HolderLookup.Provider registries;

        public Holder(HolderLookup.Provider registries) {
            this.registries = registries;
        }
        public HolderLookup.Provider lookup() {
            return registries;
        }
        public LootTable getLootTable(ResourceKey<LootTable> id) {
            return LootTable.of(id.identifier());
        }
    }
}
