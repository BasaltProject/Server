// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
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
import com.mojang.datafixers.DataFixer;
import it.unimi.dsi.fastutil.longs.LongSet;
import java.io.IOException;
import java.util.concurrent.Executor;
import java.util.function.BooleanSupplier;
import java.util.function.Predicate;
import net.minecraft.core.SectionPos;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.game.ClientGamePacketListener;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.ai.village.poi.PoiManager;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LightLayer;
import net.minecraft.world.level.NaturalSpawner;
import net.minecraft.world.level.chunk.ChunkGeneratorStructureState;
import net.minecraft.world.level.chunk.LightChunk;
import net.minecraft.world.level.chunk.storage.ChunkScanAccess;
import net.minecraft.world.level.entity.ChunkStatusUpdateListener;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplateManager;
import net.minecraft.world.level.storage.LevelStorageSource;

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
        this.lightEngine = new ThreadedLevelLightEngine(world, level);
        this.chunkMap = new ChunkMap(this, world);
        kAll.add(this);
    }

    public static void forget(String world) {
        kAll.removeIf(cache -> world.equals(cache.world));
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
            found = org.basaltmc.bukkit.facade.Memo.get(chunks, key, k -> new LevelChunk(level, chunkX, chunkZ));
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
        return new org.basaltmc.bukkit.facade.NativeChunkGenerator(level);
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
    public ServerChunkCache(ServerLevel level, LevelStorageSource.LevelStorageAccess levelStorage, DataFixer fixerUpper, StructureTemplateManager structureTemplateManager, Executor executor, ChunkGenerator generator, int viewDistance, int simulationDistance, boolean syncWrites, ChunkStatusUpdateListener chunkStatusListener) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ServerChunkCache#<init>");
    }
    public ChunkAccess getChunk(int x, int z, ChunkStatus targetStatus, boolean loadOrGenerate) {
        return getChunk(x, z, loadOrGenerate);
    }
    public LevelChunk getChunkNow(int x, int z) {
        return getChunk(x, z, false);
    }
    public CompletableFuture<ChunkResult<ChunkAccess>> getChunkFuture(int x, int z, ChunkStatus targetStatus, boolean loadOrGenerate) {
        return getChunkFutureMainThread(x, z, targetStatus, loadOrGenerate);
    }
    public boolean hasChunk(int x, int z) {
        return WorldNatives.chunkLoaded(world, x, z);
    }
    public LightChunk getChunkForLighting(int x, int z) {
        return getChunk(x, z, false);
    }
    public Level getLevel() {
        return level;
    }
    boolean runDistanceManagerUpdates() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ServerChunkCache#runDistanceManagerUpdates");
    }
    public boolean isPositionTicking(long chunkKey) {
        return WorldNatives.chunkLoaded(world, ChunkPos.getX(chunkKey), ChunkPos.getZ(chunkKey));
    }
    public void save(boolean flushStorage) {
        WorldNatives.saveWorld(world);
    }
    public void close() throws IOException {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ServerChunkCache#close");
    }
    public void tick(BooleanSupplier haveTime, boolean tickChunks) {

    }
    public String gatherStats() {
        return Integer.toString(getLoadedChunksCount());
    }
    public int getPendingTasksCount() {
        return 0;
    }
    public ChunkGeneratorStructureState getGeneratorState() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ServerChunkCache#getGeneratorState");
    }
    public int getLoadedChunksCount() {
        int[] loaded = WorldNatives.loadedChunks(world);
        return loaded == null ? 0 : loaded.length / 2;
    }
    public void onLightUpdate(LightLayer layer, SectionPos pos) {

    }
    public boolean hasActiveTickets() {
        return false;
    }
    public void addTicket(Ticket ticket, ChunkPos pos) {
        return;
    }
    public CompletableFuture<?> addTicketAndLoadWithRadius(TicketType type, ChunkPos pos, int radius) {
        addTicketWithRadius(type, pos, radius);
        return CompletableFuture.completedFuture(null);
    }
    public void removeTicketWithRadius(TicketType type, ChunkPos pos, int radius) {
        WorldNatives.releaseChunks(world, ticketToken(type, pos));
    }
    public boolean updateChunkForced(ChunkPos pos, boolean forced) {
        level.getWorld().setChunkForceLoaded(pos.x(), pos.z(), forced);
        return true;
    }
    public LongSet getForceLoadedChunks() {
        return level.getForceLoadedChunks();
    }
    public void move(ServerPlayer player) {

    }
    public boolean hasEntityWithId(int id) {
        return level.getEntity(id) != null;
    }
    public void removeEntity(Entity entity) {
        return;
    }
    private java.util.List<ServerPlayer> trackers(Entity entity) {
        java.util.List<ServerPlayer> found = new java.util.ArrayList<>();
        org.bukkit.Location at = new org.bukkit.Location(level.getWorld(), entity.getX(), entity.getY(), entity.getZ());
        for (org.bukkit.entity.Player player : level.getWorld().getPlayers()) {
            if (player instanceof org.bukkit.craftbukkit.entity.CraftPlayer craft && craft.getHandle() != entity
                    && player.getLocation().distanceSquared(at) <= 128.0 * 128.0) {
                found.add(craft.getHandle());
            }
        }
        return found;
    }
    public void addEntity(Entity entity) {
        return;
    }
    public void sendToTrackingPlayersAndSelf(Entity entity, Packet<? super ClientGamePacketListener> packet) {
        sendToTrackingPlayersFiltered(entity, packet, player -> true);
        if (entity instanceof ServerPlayer self && !trackers(entity).contains(self)) {
            self.connection.send(packet);
        }
    }
    public void sendToTrackingPlayers(Entity entity, Packet<? super ClientGamePacketListener> packet) {
        sendToTrackingPlayersFiltered(entity, packet, player -> true);
    }
    public void sendToTrackingPlayersFiltered(Entity entity, Packet<? super ClientGamePacketListener> packet, Predicate<ServerPlayer> targetPredicate) {
        for (ServerPlayer player : trackers(entity)) {
            if (targetPredicate.test(player)) {
                player.connection.send(packet);
            }
        }
    }
    public void setViewDistance(int newDistance) {
        level.getWorld().setViewDistance(newDistance);
    }
    public void setSimulationDistance(int simulationDistance) {
        level.getWorld().setSimulationDistance(simulationDistance);
    }
    public void setSpawnSettings(boolean spawnEnemies) {

    }
    public String getChunkDebugData(ChunkPos pos) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ServerChunkCache#getChunkDebugData");
    }
    public PoiManager getPoiManager() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ServerChunkCache#getPoiManager");
    }
    public ChunkScanAccess chunkScanner() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ServerChunkCache#chunkScanner");
    }
    public NaturalSpawner. SpawnState getLastSpawnState() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ServerChunkCache#getLastSpawnState");
    }
    public void deactivateTicketsOnClosing() {

    }
    public void onChunkReadyToSend(ChunkHolder chunk) {

    }
    final class MainThreadExecutor extends BlockableEventLoop<Runnable> {
        private MainThreadExecutor(Level level) {
            super((String) null, false); throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ServerChunkCache.MainThreadExecutor#<init>");
        }
        public Runnable wrapRunnable(Runnable runnable) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ServerChunkCache.MainThreadExecutor#wrapRunnable");
        }
        protected boolean shouldRun(Runnable task) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ServerChunkCache.MainThreadExecutor#shouldRun");
        }
        protected boolean scheduleExecutables() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ServerChunkCache.MainThreadExecutor#scheduleExecutables");
        }
        protected Thread getRunningThread() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ServerChunkCache.MainThreadExecutor#getRunningThread");
        }
        protected void doRunTask(Runnable task) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ServerChunkCache.MainThreadExecutor#doRunTask");
        }
        protected boolean pollTask() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ServerChunkCache.MainThreadExecutor#pollTask");
        }
    }
}
