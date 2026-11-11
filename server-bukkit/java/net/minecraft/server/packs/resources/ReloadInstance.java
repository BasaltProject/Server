// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs.resources;
import java.util.concurrent.CompletableFuture;
public interface ReloadInstance {
    CompletableFuture<?> done();
    float getActualProgress();
    default boolean isDone() {
        return done().isDone();
    }
    default void checkExceptions() {
        CompletableFuture<?> future = done();
        if (future.isCompletedExceptionally()) {
            future.join();
        }
    }
}
