// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

import java.util.concurrent.CompletableFuture;
import net.minecraft.world.level.chunk.LevelChunk;
import net.minecraft.world.level.chunk.ChunkAccess;
import java.util.List;
import java.util.concurrent.Executor;
import java.util.function.IntConsumer;
import java.util.function.IntSupplier;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.LightLayer;
import net.minecraft.world.level.lighting.LevelLightEngine;

public class ChunkHolder extends GenerationChunkHolder {
    public static ChunkResult<LevelChunk> UNLOADED_LEVEL_CHUNK = ChunkResult.error("Chunk not there");
    private LevelChunk chunk;
    private net.minecraft.world.level.LevelHeightAccessor heightAccessor;
    private int ticketLevel = 31;

    protected ChunkHolder() {
        this.chunk = null;
    }

    ChunkHolder(LevelChunk chunk) {
        super(chunk.getPos());
        this.chunk = chunk;
    }

    private LevelChunk chunk() {
        if (chunk == null && heightAccessor instanceof ServerLevel level) {
            chunk = level.getChunkSource().getChunkNow(pos.x(), pos.z());
        }
        return chunk;
    }

    @Override
    public ChunkAccess getLatestChunk() {
        return chunk();
    }

    public CompletableFuture<ChunkResult<LevelChunk>> getTickingChunkFuture() {
        LevelChunk current = chunk();
        if (current == null) {
            return CompletableFuture.completedFuture(UNLOADED_LEVEL_CHUNK);
        }
        current.flush();
        return CompletableFuture.completedFuture(ChunkResult.of(current));
    }

    public boolean wasAccessibleSinceLastSave() {
        return true;
    }

    public interface PlayerProvider {
            default List<ServerPlayer> getPlayers(ChunkPos pos, boolean borderOnly) {
            return List.of();
        }
}
    public ChunkHolder(ChunkPos pos, int ticketLevel, LevelHeightAccessor levelHeightAccessor, LevelLightEngine lightEngine, LevelChangeListener onLevelChange, PlayerProvider playerProvider) {
        super(pos);
        this.ticketLevel = ticketLevel;
        this.heightAccessor = levelHeightAccessor;
    }
    public CompletableFuture<ChunkResult<LevelChunk>> getEntityTickingChunkFuture() {
        return getTickingChunkFuture();
    }
    public CompletableFuture<ChunkResult<LevelChunk>> getFullChunkFuture() {
        return getTickingChunkFuture();
    }
    public LevelChunk getTickingChunk() {
        return chunk();
    }
    public LevelChunk getChunkToSend() {
        return chunk();
    }
    public CompletableFuture<?> getSendSyncFuture() {
        return CompletableFuture.completedFuture(null);
    }
    public void addSendDependency(CompletableFuture<?> sync) {
        return;
    }
    public CompletableFuture<?> getSaveSyncFuture() {
        return CompletableFuture.completedFuture(null);
    }
    public boolean isReadyForSaving() {
        return true;
    }
    protected void addSaveDependency(CompletableFuture<?> sync) {
        return;
    }
    public boolean blockChanged(BlockPos pos) {
        return chunk() != null;
    }
    public boolean sectionLightChanged(LightLayer layer, int chunkY) {
        return false;
    }
    public boolean hasChangesToBroadcast() {
        return false;
    }
    public void broadcastChanges(LevelChunk chunk) {
        return;
    }
    public int getTicketLevel() {
        return ticketLevel;
    }
    public int getQueueLevel() {
        return ticketLevel;
    }
    public void setTicketLevel(int ticketLevel) {
        this.ticketLevel = ticketLevel;
    }
    protected void updateFutures(ChunkMap scheduler, Executor mainThreadExecutor) {
        return;
    }
    public void refreshAccessibility() {
        return;
    }
    public interface LevelChangeListener {
        void onLevelChange(ChunkPos pos, IntSupplier oldLevel, int newLevel, IntConsumer setQueueLevel);
    }
}
