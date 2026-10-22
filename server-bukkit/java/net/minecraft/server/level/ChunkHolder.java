package net.minecraft.server.level;

import java.util.concurrent.CompletableFuture;
import org.basaltmc.bukkit.Unsupported;
public class ChunkHolder extends GenerationChunkHolder {
    protected ChunkHolder() {
    }
    public static ChunkResult UNLOADED_LEVEL_CHUNK;
    public CompletableFuture getTickingChunkFuture() {
        throw Unsupported.hit("net.minecraft.server.level.ChunkHolder.getTickingChunkFuture");
    }

    public interface PlayerProvider {
    }
}
