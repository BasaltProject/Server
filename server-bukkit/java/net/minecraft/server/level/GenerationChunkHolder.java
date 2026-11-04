// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

import com.mojang.datafixers.util.Pair;
import java.util.List;
import java.util.concurrent.CompletableFuture;
import net.minecraft.util.StaticCache2D;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.chunk.ChunkAccess;
import net.minecraft.world.level.chunk.ImposterProtoChunk;
import net.minecraft.world.level.chunk.status.ChunkStatus;
import net.minecraft.world.level.chunk.status.ChunkStep;
public abstract class GenerationChunkHolder {
    protected GenerationChunkHolder() {
        this.pos = null;
    }

    private CompletableFuture<ChunkResult<ChunkAccess>> completedChunk() {
        ChunkAccess chunk = getLatestChunk();
        return CompletableFuture.completedFuture(chunk == null ? ChunkResult.error("Chunk not there") : ChunkResult.of(chunk));
    }
    public static final ChunkResult<ChunkAccess> UNLOADED_CHUNK = ChunkResult.error("Unloaded chunk");
    public static final CompletableFuture<ChunkResult<ChunkAccess>> UNLOADED_CHUNK_FUTURE = CompletableFuture.completedFuture(UNLOADED_CHUNK);
    protected final ChunkPos pos;
    private final java.util.concurrent.atomic.AtomicInteger generationRefs = new java.util.concurrent.atomic.AtomicInteger();
    public GenerationChunkHolder(ChunkPos pos) {
        this.pos = pos;
    }
    public CompletableFuture<ChunkResult<ChunkAccess>> scheduleChunkGenerationTask(ChunkStatus status, ChunkMap scheduler) {
        return completedChunk();
    }
    CompletableFuture<ChunkResult<ChunkAccess>> applyStep(ChunkStep step, GeneratingChunkMap chunkMap, StaticCache2D<GenerationChunkHolder> cache) {
        return completedChunk();
    }
    protected void updateHighestAllowedStatus(ChunkMap scheduler) {
        return;
    }
    public void replaceProtoChunk(ImposterProtoChunk chunk) {
        return;
    }
    void removeTask(ChunkGenerationTask task) {
        return;
    }
    protected void addSaveDependency(CompletableFuture<?> sync) {
        return;
    }
    public void increaseGenerationRefCount() {
        generationRefs.incrementAndGet();
    }
    public void decreaseGenerationRefCount() {
        generationRefs.decrementAndGet();
    }
    public ChunkAccess getChunkIfPresentUnchecked(ChunkStatus status) {
        return getLatestChunk();
    }
    public ChunkAccess getChunkIfPresent(ChunkStatus status) {
        return getLatestChunk();
    }
    public ChunkAccess getLatestChunk() {
        return null;
    }
    public ChunkStatus getPersistedStatus() {
        return ChunkStatus.FULL;
    }
    public ChunkPos getPos() {
        return pos;
    }
    public FullChunkStatus getFullStatus() {
        return FullChunkStatus.ENTITY_TICKING;
    }
    public int getTicketLevel() {
        return 31;
    }
    public int getQueueLevel() {
        return 31;
    }
    public List<Pair<ChunkStatus, CompletableFuture<ChunkResult<ChunkAccess>>>> getAllFutures() {
        return List.of();
    }
    public ChunkStatus getLatestStatus() {
        return ChunkStatus.FULL;
    }
}
