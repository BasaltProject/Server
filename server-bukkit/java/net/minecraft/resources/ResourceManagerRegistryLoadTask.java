// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.resources;

import com.mojang.serialization.Lifecycle;
import java.util.Map;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Executor;
import net.minecraft.server.packs.resources.ResourceManager;

public class ResourceManagerRegistryLoadTask<T> extends RegistryLoadTask<T> {
    private final ResourceManager resourceManager;

    public ResourceManagerRegistryLoadTask(RegistryDataLoader.RegistryData<T> data, Lifecycle lifecycle, Map<ResourceKey<?>, Exception> loadingErrors,
            ResourceManager resourceManager) {
        super(data, lifecycle, loadingErrors);
        this.resourceManager = resourceManager;
    }

    @Override
    public CompletableFuture<?> load(RegistryOps.RegistryInfoLookup context, Executor executor) {
        return CompletableFuture.completedFuture(resourceManager);
    }
}
