// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.registries;

import java.util.concurrent.CompletableFuture;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.RegistrySetBuilder;

public class RegistryPatchGenerator {
    private static final HolderLookup.Provider kEmpty = HolderLookup.Provider.create(java.util.stream.Stream.empty());

    public static CompletableFuture<RegistrySetBuilder.PatchedRegistries> createWorldLookup(
            CompletableFuture<HolderLookup.Provider> vanilla, RegistrySetBuilder packBuilder) {
        return vanilla.thenApply(base -> new RegistrySetBuilder.PatchedRegistries(base, kEmpty));
    }

    public static CompletableFuture<RegistrySetBuilder.PatchedRegistries> createReloadableLookup(
            CompletableFuture<HolderLookup.Provider> contextFuture, CompletableFuture<HolderLookup.Provider> vanillaFuture,
            RegistrySetBuilder packBuilder) {
        return vanillaFuture.thenApply(base -> new RegistrySetBuilder.PatchedRegistries(base, kEmpty));
    }
}
