// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs.resources;
import net.minecraft.util.profiling.Profiler;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Executor;
import net.minecraft.util.profiling.ProfilerFiller;
public abstract class SimplePreparableReloadListener<T> implements PreparableReloadListener {
    public final CompletableFuture<Void> reload(PreparableReloadListener.SharedState currentReload, Executor taskExecutor, PreparableReloadListener.PreparationBarrier preparationBarrier, Executor reloadExecutor) {
        ResourceManager manager = currentReload.resourceManager();
        return CompletableFuture.supplyAsync(() -> prepare(manager, Profiler.get()), taskExecutor)
            .thenCompose(preparationBarrier::wait)
            .thenAcceptAsync(prepared -> apply(prepared, manager, Profiler.get()), reloadExecutor);
    }
    protected abstract T prepare(ResourceManager manager, ProfilerFiller profiler);
    protected abstract void apply(T preparations, ResourceManager manager, ProfilerFiller profiler);
}
