// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.jsonrpc.internalapi;
import net.minecraft.server.MinecraftServer;
import java.util.concurrent.CompletableFuture;
import java.util.function.Supplier;
import net.minecraft.server.notifications.NotificationManager;
public class MinecraftExecutorServiceImpl implements MinecraftExecutorService {
    public MinecraftExecutorServiceImpl(NotificationManager notificationManager) {
        
    }
    public <V> CompletableFuture<V> submit(Supplier<V> supplier) {
        CompletableFuture<V> future = new CompletableFuture<>();
        MinecraftServer.getServer().execute(() -> {
            try {
                future.complete(supplier.get());
            } catch (Throwable failure) {
                future.completeExceptionally(failure);
            }
        });
        return future;
    }
    public CompletableFuture<Void> submit(Runnable runnable) {
        return submit(() -> {
            runnable.run();
            return null;
        });
    }
}
