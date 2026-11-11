// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.resources;

import com.mojang.serialization.Lifecycle;
import java.util.Map;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Executor;
import net.minecraft.core.Registry;

public class NetworkRegistryLoadTask<T> extends RegistryLoadTask<T> {
    private final RegistryDataLoader.NetworkedRegistryData networkedData;

    public NetworkRegistryLoadTask(RegistryDataLoader.RegistryData<T> data, Lifecycle lifecycle, Map<ResourceKey<?>, Exception> loadingErrors,
            Map<ResourceKey<? extends Registry<?>>, RegistryDataLoader.NetworkedRegistryData> networkedRegistries) {
        super(data, lifecycle, loadingErrors);
        this.networkedData = networkedRegistries.get(data.key());
    }

    @Override
    public CompletableFuture<?> load(RegistryOps.RegistryInfoLookup context, Executor executor) {
        return CompletableFuture.completedFuture(networkedData);
    }
}
