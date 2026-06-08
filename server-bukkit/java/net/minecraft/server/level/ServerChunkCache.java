package net.minecraft.server.level;

import java.util.Map;
import java.util.Set;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ConcurrentHashMap;
import net.minecraft.core.BlockPos;
import net.minecraft.server.MinecraftServer;
import net.minecraft.util.thread.BlockableEventLoop;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.chunk.ChunkAccess;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.chunk.ChunkSource;
import net.minecraft.world.level.chunk.LevelChunk;
import net.minecraft.world.level.chunk.status.ChunkStatus;
import net.minecraft.world.level.levelgen.RandomState;
import net.minecraft.world.level.storage.SavedDataStorage;
import org.basaltmc.bukkit.Unsupported;
import org.basaltmc.bukkit.WorldNatives;

public class ServerChunkCache extends ChunkSource {
    private static final Set<ServerChunkCache> kAll = ConcurrentHashMap.newKeySet();
    private static final int kCapacity = 2048;
    private static final int kResendGraceTicks = 3;
    private static final int kBlockChangeLimit = 64;

    public ChunkMap chunkMap;
    public BlockableEventLoop<Runnable> mainThreadProcessor;

    private final ServerLevel level;
    private final String world;
    private final Map<Long, LevelChunk> chunks = new ConcurrentHashMap<>();
    private final Set<LevelChunk> handed = ConcurrentHashMap.newKeySet();
    private final Map<Long, Integer> unsent = new ConcurrentHashMap<>();
    private final java.util.List<Integer> changedBlocks = new java.util.ArrayList<>();
    private final ThreadedLevelLightEngine lightEngine;

    protected ServerChunkCache() {
        this.level = null;
        this.world = null;
        this.lightEngine = null;
    }

    ServerChunkCache(ServerLevel level) {
        this.level = level;
        this.world = level.getWorld().getName();
        this.lightEngine = new ThreadedLevelLightEngine(world);
        this.chunkMap = new ChunkMap(this, world);
        kAll.add(this);
    }

    public static void tickAll() {
        for (ServerChunkCache cache : kAll) {
            cache.tick();
        }
    }

    private void tick() {
        for (LevelChunk chunk : handed) {
            handed.remove(chunk);
            chunk.flush();
        }
        flushBlockChanges();
        lightEngine.flush();
        if (unsent.isEmpty()) {
            return;
        }
        int now = MinecraftServer.currentTick;
        for (Map.Entry<Long, Integer> entry : unsent.entrySet()) {
            if (now - entry.getValue() >= kResendGraceTicks && unsent.remove(entry.getKey(), entry.getValue())) {
                ChunkPos pos = ChunkPos.unpack(entry.getKey());
                WorldNatives.resendChunks(world, new int[] {pos.x(), pos.z()});
            }
        }
    }

    public void noteHanded(LevelChunk chunk) {
        handed.add(chunk);
    }

    public void noteChanged(int chunkX, int chunkZ) {
        unsent.put(ChunkPos.pack(chunkX, chunkZ), MinecraftServer.currentTick);
    }

    public void resend(int chunkX, int chunkZ) {
        unsent.remove(ChunkPos.pack(chunkX, chunkZ));
        WorldNatives.resendChunks(world, new int[] {chunkX, chunkZ});
    }

    public LevelChunk getChunk(int chunkX, int chunkZ, boolean load) {
        long key = ChunkPos.pack(chunkX, chunkZ);
        boolean loaded = WorldNatives.chunkLoaded(world, chunkX, chunkZ);
        if (!loaded) {
            chunks.remove(key);
            if (!load || !WorldNatives.loadChunk(world, chunkX, chunkZ, true)) {
                return null;
            }
        }
        LevelChunk found = chunks.get(key);
        if (found == null) {
            if (chunks.size() >= kCapacity) {
                chunks.values().removeIf(chunk -> !chunk.hasUnflushed());
            }
            found = chunks.computeIfAbsent(key, k -> new LevelChunk(level, chunkX, chunkZ));
        }
        return found;
    }

    public LevelChunk getChunkAtIfCachedImmediately(int chunkX, int chunkZ) {
        LevelChunk found = chunks.get(ChunkPos.pack(chunkX, chunkZ));
        return found != null && WorldNatives.chunkLoaded(world, chunkX, chunkZ) ? found : null;
    }

    public LevelChunk getChunkAtIfLoadedImmediately(int chunkX, int chunkZ) {
        return getChunk(chunkX, chunkZ, false);
    }

    public CompletableFuture<ChunkResult<ChunkAccess>> getChunkFutureMainThread(int x, int z, ChunkStatus status,
            boolean load) {
        LevelChunk chunk = getChunk(x, z, load);
        return CompletableFuture.completedFuture(chunk == null ? ChunkResult.error("not loaded") : ChunkResult.of(chunk));
    }

    public void addTicketAtLevel(TicketType ticketType, ChunkPos chunkPos, int value) {
        addTicketWithRadius(ticketType, chunkPos, 0);
    }

    public void addTicketWithRadius(TicketType type, ChunkPos pos, int radius) {
        int reach = Math.max(radius, 0);
        int[] chunks = new int[(2 * reach + 1) * (2 * reach + 1) * 2];
        int at = 0;
        for (int x = -reach; x <= reach; x++) {
            for (int z = -reach; z <= reach; z++) {
                chunks[at++] = pos.x() + x;
                chunks[at++] = pos.z() + z;
            }
        }
        WorldNatives.holdChunks(world, ticketToken(type, pos), chunks);
    }

    public void removeTicketAtLevel(TicketType ticketType, ChunkPos chunkPos, int value) {
        WorldNatives.releaseChunks(world, ticketToken(ticketType, chunkPos));
    }

    private static String ticketToken(TicketType type, ChunkPos pos) {
        return "ticket:" + type + ":" + pos.x() + "," + pos.z();
    }

    public void blockChanged(BlockPos pos) {
        synchronized (changedBlocks) {
            changedBlocks.add(pos.getX());
            changedBlocks.add(pos.getY());
            changedBlocks.add(pos.getZ());
        }
    }

    private void flushBlockChanges() {
        int[] blocks;
        synchronized (changedBlocks) {
            if (changedBlocks.isEmpty()) {
                return;
            }
            blocks = changedBlocks.stream().mapToInt(Integer::intValue).toArray();
            changedBlocks.clear();
        }
        Map<Long, Integer> perChunk = new java.util.HashMap<>();
        for (int i = 0; i < blocks.length; i += 3) {
            perChunk.merge(ChunkPos.pack(blocks[i] >> 4, blocks[i + 2] >> 4), 1, Integer::sum);
        }
        int[] single = new int[blocks.length];
        int kept = 0;
        for (int i = 0; i < blocks.length; i += 3) {
            long key = ChunkPos.pack(blocks[i] >> 4, blocks[i + 2] >> 4);
            if (perChunk.get(key) > kBlockChangeLimit) {
                continue;
            }
            System.arraycopy(blocks, i, single, kept, 3);
            kept += 3;
        }
        if (kept > 0) {
            WorldNatives.updateNeighbors(world, java.util.Arrays.copyOf(single, kept), 64);
        }
        for (Map.Entry<Long, Integer> entry : perChunk.entrySet()) {
            if (entry.getValue() > kBlockChangeLimit) {
                ChunkPos pos = ChunkPos.unpack(entry.getKey());
                resend(pos.x(), pos.z());
            }
        }
    }

    public void close(boolean flag) {}

    public SavedDataStorage getDataStorage() {
        throw Unsupported.hit("net.minecraft.server.level.ServerChunkCache.getDataStorage");
    }

    public ChunkGenerator getGenerator() {
        throw Unsupported.hit("net.minecraft.server.level.ServerChunkCache.getGenerator");
    }

    public ThreadedLevelLightEngine getLightEngine() {
        return lightEngine;
    }

    public boolean pollTask() {
        return false;
    }

    public RandomState randomState() {
        throw Unsupported.hit("net.minecraft.server.level.ServerChunkCache.randomState");
    }
}
