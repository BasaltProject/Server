package net.minecraft.server.level;

import java.util.concurrent.CompletableFuture;
import net.minecraft.world.level.chunk.LevelChunk;

public class ChunkHolder extends GenerationChunkHolder {
    public static ChunkResult<LevelChunk> UNLOADED_LEVEL_CHUNK = ChunkResult.error("Chunk not there");
    private final LevelChunk chunk;

    protected ChunkHolder() {
        this.chunk = null;
    }

    ChunkHolder(LevelChunk chunk) {
        this.chunk = chunk;
    }

    public CompletableFuture<ChunkResult<LevelChunk>> getTickingChunkFuture() {
        chunk.flush();
        return CompletableFuture.completedFuture(ChunkResult.of(chunk));
    }

    public boolean wasAccessibleSinceLastSave() {
        return true;
    }

    public interface PlayerProvider {
    }
}
