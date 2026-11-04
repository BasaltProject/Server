// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

import java.util.concurrent.CompletableFuture;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.chunk.status.ChunkStatus;

public class ChunkGenerationTask {
    public final ChunkStatus targetStatus;
    private final GeneratingChunkMap chunkMap;
    private final ChunkPos pos;
    private volatile boolean cancelled;

    private ChunkGenerationTask(GeneratingChunkMap chunkMap, ChunkStatus targetStatus, ChunkPos pos) {
        this.chunkMap = chunkMap;
        this.targetStatus = targetStatus;
        this.pos = pos;
    }

    public static ChunkGenerationTask create(GeneratingChunkMap chunkMap, ChunkStatus targetStatus, ChunkPos pos) {
        return new ChunkGenerationTask(chunkMap, targetStatus, pos);
    }

    public CompletableFuture<?> runUntilWait() {
        if (cancelled) {
            return CompletableFuture.completedFuture(null);
        }
        return getCenter().scheduleChunkGenerationTask(targetStatus, chunkMap instanceof ChunkMap map ? map : null);
    }

    public void markForCancellation() {
        cancelled = true;
    }

    public GenerationChunkHolder getCenter() {
        return chunkMap.acquireGeneration(pos.pack());
    }
}
