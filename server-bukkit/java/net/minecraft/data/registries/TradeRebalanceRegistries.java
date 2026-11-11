// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.registries;

import java.util.concurrent.CompletableFuture;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.RegistrySetBuilder;

public class TradeRebalanceRegistries {
    public static CompletableFuture<RegistrySetBuilder.PatchedRegistries> createPatchedWorldRegistries(
            CompletableFuture<HolderLookup.Provider> vanillaWorld) {
        return RegistryPatchGenerator.createWorldLookup(vanillaWorld, new RegistrySetBuilder());
    }

    public static CompletableFuture<RegistrySetBuilder.PatchedRegistries> createPatchedReloadable(
            CompletableFuture<HolderLookup.Provider> context, CompletableFuture<HolderLookup.Provider> vanilla) {
        return RegistryPatchGenerator.createReloadableLookup(context, vanilla, new RegistrySetBuilder());
    }
}
