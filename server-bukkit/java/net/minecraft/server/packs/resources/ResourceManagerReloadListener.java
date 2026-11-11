// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs.resources;
import net.minecraft.util.Unit;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Executor;
public interface ResourceManagerReloadListener extends PreparableReloadListener {
    default CompletableFuture<Void> reload(PreparableReloadListener.SharedState currentReload, Executor taskExecutor, PreparableReloadListener.PreparationBarrier preparationBarrier, Executor reloadExecutor) {
        ResourceManager manager = currentReload.resourceManager();
        return preparationBarrier.wait(Unit.INSTANCE).thenRunAsync(() -> onResourceManagerReload(manager), reloadExecutor);
    }
    void onResourceManagerReload(ResourceManager resourceManager);
}
