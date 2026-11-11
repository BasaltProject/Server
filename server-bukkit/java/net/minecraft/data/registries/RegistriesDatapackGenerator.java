// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.registries;

import java.util.Collection;
import java.util.List;
import java.util.concurrent.CompletableFuture;
import net.minecraft.core.HolderLookup;
import net.minecraft.data.CachedOutput;
import net.minecraft.data.DataProvider;
import net.minecraft.data.NativeData;
import net.minecraft.data.PackOutput;
import net.minecraft.resources.RegistryDataLoader;

public class RegistriesDatapackGenerator implements DataProvider {
    private final PackOutput output;
    private final String name;
    private final Collection<RegistryDataLoader.RegistryData<?>> registryData;

    public RegistriesDatapackGenerator(PackOutput output, String name,
            Collection<RegistryDataLoader.RegistryData<?>> registryData, CompletableFuture<HolderLookup.Provider> registryContents) {
        this.output = output;
        this.name = name;
        this.registryData = registryData;
    }

    public static RegistriesDatapackGenerator forWorldLayer(PackOutput output, CompletableFuture<HolderLookup.Provider> registryContents) {
        return new RegistriesDatapackGenerator(output, "Registries", RegistryDataLoader.WORLD_REGISTRIES, registryContents);
    }

    public static DataProvider forReloadableLayer(PackOutput output, CompletableFuture<HolderLookup.Provider> registryContents) {
        return new RegistriesDatapackGenerator(output, "Reloadable Registries", RegistryDataLoader.RELOADABLE_REGISTRIES, registryContents);
    }

    @Override
    public CompletableFuture<?> run(CachedOutput cache) {
        List<CompletableFuture<?>> writes = new java.util.ArrayList<>();
        for (RegistryDataLoader.RegistryData<?> data : registryData) {
            writes.add(NativeData.copyFolder(cache, output, data.key().identifier().getPath()));
        }
        return CompletableFuture.allOf(writes.toArray(CompletableFuture[]::new));
    }

    @Override
    public final String getName() {
        return name;
    }
}
