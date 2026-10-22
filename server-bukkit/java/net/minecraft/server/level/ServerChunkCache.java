package net.minecraft.server.level;

import net.minecraft.core.BlockPos;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.chunk.ChunkSource;
import net.minecraft.world.level.chunk.LevelChunk;
import net.minecraft.world.level.levelgen.RandomState;
import net.minecraft.world.level.storage.SavedDataStorage;
import java.util.concurrent.CompletableFuture;
import net.minecraft.util.thread.BlockableEventLoop;
import net.minecraft.world.level.chunk.ChunkAccess;
import net.minecraft.world.level.chunk.status.ChunkStatus;
import org.basaltmc.bukkit.Unsupported;
public class ServerChunkCache extends ChunkSource {
    protected ServerChunkCache() {
    }
    public ChunkMap chunkMap;
    public BlockableEventLoop<Runnable> mainThreadProcessor;

    public CompletableFuture<ChunkResult<ChunkAccess>> getChunkFutureMainThread(int x, int z, ChunkStatus status,
            boolean load) {
        throw Unsupported.hit("net.minecraft.server.level.ServerChunkCache.getChunkFutureMainThread");
    }
    public void addTicketAtLevel(TicketType ticketType, ChunkPos chunkPos, int value) {
        throw Unsupported.hit("net.minecraft.server.level.ServerChunkCache.addTicketAtLevel");
    }
    public void addTicketWithRadius(TicketType type, ChunkPos pos, int radius) {
        throw Unsupported.hit("net.minecraft.server.level.ServerChunkCache.addTicketWithRadius");
    }
    public void blockChanged(BlockPos pos) {
        throw Unsupported.hit("net.minecraft.server.level.ServerChunkCache.blockChanged");
    }
    public void close(boolean flag) {
        throw Unsupported.hit("net.minecraft.server.level.ServerChunkCache.close");
    }
    public LevelChunk getChunk(int value, int value2, boolean flag) {
        throw Unsupported.hit("net.minecraft.server.level.ServerChunkCache.getChunk");
    }
    public LevelChunk getChunkAtIfCachedImmediately(int value, int value2) {
        throw Unsupported.hit("net.minecraft.server.level.ServerChunkCache.getChunkAtIfCachedImmediately");
    }
    public LevelChunk getChunkAtIfLoadedImmediately(int value, int value2) {
        throw Unsupported.hit("net.minecraft.server.level.ServerChunkCache.getChunkAtIfLoadedImmediately");
    }
    public SavedDataStorage getDataStorage() {
        throw Unsupported.hit("net.minecraft.server.level.ServerChunkCache.getDataStorage");
    }
    public ChunkGenerator getGenerator() {
        throw Unsupported.hit("net.minecraft.server.level.ServerChunkCache.getGenerator");
    }
    public ThreadedLevelLightEngine getLightEngine() {
        throw Unsupported.hit("net.minecraft.server.level.ServerChunkCache.getLightEngine");
    }
    public boolean pollTask() {
        throw Unsupported.hit("net.minecraft.server.level.ServerChunkCache.pollTask");
    }
    public RandomState randomState() {
        throw Unsupported.hit("net.minecraft.server.level.ServerChunkCache.randomState");
    }
    public void removeTicketAtLevel(TicketType ticketType, ChunkPos chunkPos, int value) {
        throw Unsupported.hit("net.minecraft.server.level.ServerChunkCache.removeTicketAtLevel");
    }
}
