package org.bukkit.craftbukkit;

import java.util.concurrent.CompletableFuture;
import net.minecraft.server.level.ServerLevel;
import org.basaltmc.bukkit.gen.WorldBase;
import org.basaltmc.bukkit.facade.TickQueue;
import org.bukkit.Chunk;

/** What plugins cast a World to; the level behind it is made on first use. */
public abstract class CraftWorld extends WorldBase {
    private ServerLevel handle;

    protected CraftWorld() {}

    public synchronized ServerLevel getHandle() {
        if (handle == null) {
            handle = new ServerLevel(this);
        }
        return handle;
    }

    public CompletableFuture<Chunk> getChunkAtAsync(int x, int z) {
        return getChunkAtAsync(x, z, true, false);
    }

    public CompletableFuture<Chunk> getChunkAtAsync(int x, int z, boolean generate, boolean urgent) {
        CompletableFuture<Chunk> future = new CompletableFuture<>();
        TickQueue.run(() -> {
            try {
                future.complete(getChunkAt(x, z, generate));
            } catch (RuntimeException e) {
                future.completeExceptionally(e);
            }
        });
        return future;
    }
}
