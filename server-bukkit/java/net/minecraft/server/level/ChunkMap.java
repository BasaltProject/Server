// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

import java.util.List;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.chunk.LevelChunk;
import net.minecraft.world.level.chunk.storage.SimpleRegionStorage;
import org.basaltmc.bukkit.facade.ChunkResender;
import com.mojang.datafixers.DataFixer;
import java.io.IOException;
import java.io.Writer;
import java.util.Optional;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Executor;
import java.util.function.BooleanSupplier;
import java.util.function.Consumer;
import java.util.function.IntFunction;
import java.util.function.IntSupplier;
import java.util.function.Predicate;
import java.util.stream.Stream;
import net.minecraft.ReportedException;
import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.game.ClientGamePacketListener;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.util.StaticCache2D;
import net.minecraft.util.thread.BlockableEventLoop;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.UpdateInterval;
import net.minecraft.world.entity.ai.village.poi.PoiManager;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.TicketStorage;
import net.minecraft.world.level.chunk.ChunkAccess;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.chunk.ChunkGeneratorStructureState;
import net.minecraft.world.level.chunk.LightChunkGetter;
import net.minecraft.world.level.chunk.status.ChunkStatus;
import net.minecraft.world.level.chunk.status.ChunkStep;
import net.minecraft.world.level.entity.ChunkStatusUpdateListener;
import net.minecraft.world.level.levelgen.RandomState;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplateManager;
import net.minecraft.world.level.storage.LevelStorageSource;

public class ChunkMap extends SimpleRegionStorage implements ChunkHolder.PlayerProvider, GeneratingChunkMap {
    private final ServerChunkCache source;
    private final ServerPlayer watcher;

    protected ChunkMap() {
        this.source = null;
        this.watcher = null;
    }

    ChunkMap(ServerChunkCache source, String world) {
        this.source = source;
        this.watcher = new ServerPlayer(new ChunkResender(world, source));
    }

    /** Looked up by name by the world editing adapters. */
    ChunkHolder getVisibleChunkIfPresent(long packed) {
        LevelChunk chunk = source.getChunk(ChunkPos.unpack(packed).x(), ChunkPos.unpack(packed).z(), false);
        return chunk == null ? null : new ChunkHolder(chunk);
    }

    public List<ServerPlayer> getPlayers(ChunkPos pos, boolean borderOnly) {
        return List.of(watcher);
    }

    public void resendBiomesForChunks(List chunks) {
        for (Object chunk : chunks) {
            if (chunk instanceof LevelChunk levelChunk) {
                source.resend(levelChunk.getPos().x(), levelChunk.getPos().z());
            }
        }
    }
    public static final int MIN_VIEW_DISTANCE = 2;
    public static final int MAX_VIEW_DISTANCE = 32;
    public static final int FORCED_TICKET_LEVEL = 0;
    public ChunkMap(ServerLevel level, LevelStorageSource.LevelStorageAccess levelStorage, DataFixer dataFixer, StructureTemplateManager structureManager, Executor executor, BlockableEventLoop<Runnable> mainThreadExecutor, LightChunkGetter chunkGetter, ChunkGenerator generator, ChunkStatusUpdateListener chunkStatusListener, TicketStorage ticketStorage, int serverViewDistance, boolean syncWrites) {
        super((net.minecraft.world.level.chunk.storage.RegionStorageInfo) null, (java.nio.file.Path) null, (com.mojang.datafixers.DataFixer) null, false, (net.minecraft.util.datafix.DataFixTypes) null); throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ChunkMap#<init>");
    }
    protected ChunkGenerator generator() {
        return source.getGenerator();
    }
    protected ChunkGeneratorStructureState generatorState() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ChunkMap#generatorState");
    }
    protected RandomState randomState() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ChunkMap#randomState");
    }
    public boolean isChunkTracked(ServerPlayer player, int chunkX, int chunkZ) {
        return ((org.bukkit.entity.Player) player.getBukkitEntity()).isChunkSent(org.bukkit.Chunk.getChunkKey(chunkX, chunkZ));
    }
    protected ThreadedLevelLightEngine getLightEngine() {
        return source.getLightEngine();
    }
    public ChunkHolder getUpdatingChunkIfPresent(long key) {
        return getVisibleChunkIfPresent(key);
    }
    public ChunkStatus getLatestStatus(long key) {
        return getVisibleChunkIfPresent(key) == null ? null : net.minecraft.world.level.chunk.status.ChunkStatus.FULL;
    }
    protected IntSupplier getChunkQueueLevel(long pos) {
        return () -> 0;
    }
    public String getChunkDebugData(ChunkPos pos) {
        return "Chunk " + pos;
    }
    CompletableFuture<ChunkResult<List<ChunkAccess>>> getChunkRangeFuture(ChunkHolder centerChunk, int range, IntFunction<ChunkStatus> distanceToStatus) {
        List<ChunkAccess> chunks = new java.util.ArrayList<>();
        ChunkPos center = centerChunk.getPos();
        for (int dz = -range; dz <= range; dz++) {
            for (int dx = -range; dx <= range; dx++) {
                chunks.add(source.getChunk(center.x() + dx, center.z() + dz, true));
            }
        }
        return CompletableFuture.completedFuture(ChunkResult.of(chunks));
    }
    public ReportedException debugFuturesAndCreateReportedException(IllegalStateException exception, String details) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ChunkMap#debugFuturesAndCreateReportedException");
    }
    public CompletableFuture<ChunkResult<LevelChunk>> prepareEntityTickingChunk(ChunkHolder chunk) {
        return chunk.getTickingChunkFuture();
    }
    public void close() throws IOException {
        
    }
    protected void saveAllChunks(boolean flushStorage) {
        return;
    }
    protected void tick(BooleanSupplier haveTime) {
        
    }
    public boolean hasWork() {
        return false;
    }
    protected boolean promoteChunkMap() {
        return false;
    }
    public GenerationChunkHolder acquireGeneration(long chunkNode) {
        LevelChunk chunk = source.getChunk(ChunkPos.unpack(chunkNode).x(), ChunkPos.unpack(chunkNode).z(), true);
        return new ChunkHolder(chunk);
    }
    public void releaseGeneration(GenerationChunkHolder chunkHolder) {
        return;
    }
    public CompletableFuture<ChunkAccess> applyStep(GenerationChunkHolder chunkHolder, ChunkStep step, StaticCache2D<GenerationChunkHolder> cache) {
        return CompletableFuture.completedFuture(chunkHolder.getLatestChunk());
    }
    public ChunkGenerationTask scheduleGenerationTask(ChunkStatus targetStatus, ChunkPos pos) {
        return ChunkGenerationTask.create(this, targetStatus, pos);
    }
    public void runGenerationTasks() {
        return;
    }
    public CompletableFuture<ChunkResult<LevelChunk>> prepareTickingChunk(ChunkHolder chunk) {
        return chunk.getTickingChunkFuture();
    }
    public CompletableFuture<ChunkResult<LevelChunk>> prepareAccessibleChunk(ChunkHolder chunk) {
        return chunk.getTickingChunkFuture();
    }
    Stream<ChunkHolder> allChunksWithAtLeastStatus(ChunkStatus status) {
        List<ChunkHolder> holders = new java.util.ArrayList<>();
        for (org.bukkit.Chunk chunk : ((ServerLevel) source.getLevel()).getWorld().getLoadedChunks()) {
            ChunkHolder holder = getVisibleChunkIfPresent(chunk.getChunkKey());
            if (holder != null) {
                holders.add(holder);
            }
        }
        return holders.stream();
    }
    protected void setServerViewDistance(int newViewDistance) {
        source.setViewDistance(newViewDistance);
    }
    public LevelChunk getChunkToSend(long key) {
        ChunkHolder holder = getVisibleChunkIfPresent(key);
        return holder == null ? null : holder.getChunkToSend();
    }
    public int size() {
        return source.getLoadedChunksCount();
    }
    public net.minecraft.server.level.DistanceManager getDistanceManager() {
        if (distanceManager == null) {
            distanceManager = new DistanceManager(null, null, null);
            distanceManager.bind(((ServerLevel) source.getLevel()).getWorld());
        }
        return distanceManager;
    }
    void dumpChunks(Writer output) throws IOException {
        for (org.bukkit.Chunk chunk : ((ServerLevel) source.getLevel()).getWorld().getLoadedChunks()) {
            output.write(chunk.getX() + "\t" + chunk.getZ() + "\tfull\n");
        }
    }
    public static CompoundTag getChunkDataFixContextTag(ResourceKey<Level> dimension, Optional<Identifier> generatorIdentifier) {
        CompoundTag tag = new CompoundTag();
        tag.putString("dimension", dimension.identifier().toString());
        generatorIdentifier.ifPresent(id -> tag.putString("generator", id.toString()));
        return tag;
    }
    public void collectSpawningChunks(List<LevelChunk> output) {
        for (org.bukkit.Chunk chunk : ((ServerLevel) source.getLevel()).getWorld().getLoadedChunks()) {
            LevelChunk handle = source.getChunk(chunk.getX(), chunk.getZ(), false);
            if (handle != null && anyPlayerCloseEnoughForSpawning(handle.getPos())) {
                output.add(handle);
            }
        }
    }
    public void forEachBlockTickingChunk(Consumer<LevelChunk> tickingChunkConsumer) {
        forEachReadyToSendChunk(tickingChunkConsumer);
    }
    private java.util.List<ServerPlayer> tracking(Entity entity) {
        java.util.List<ServerPlayer> out = new java.util.ArrayList<>();
        org.bukkit.World world = ((ServerLevel) source.getLevel()).getWorld();
        double range = Math.min(128.0, world.getViewDistance() * 16.0);
        for (org.bukkit.entity.Player player : world.getPlayers()) {
            double dx = player.getLocation().getX() - entity.getX();
            double dz = player.getLocation().getZ() - entity.getZ();
            if (dx * dx + dz * dz <= range * range) {
                out.add(((org.bukkit.craftbukkit.entity.CraftPlayer) player).getHandle());
            }
        }
        return out;
    }
    public boolean anyPlayerCloseEnoughForSpawning(ChunkPos pos) {
        return !getPlayersCloseForSpawning(pos).isEmpty();
    }
    public boolean anyPlayerCloseEnoughTo(BlockPos pos, int maxDistance) {
        for (org.bukkit.entity.Player player : ((ServerLevel) source.getLevel()).getWorld().getPlayers()) {
            if (player.getLocation().distanceSquared(new org.bukkit.Location(player.getWorld(), pos.getX() + 0.5,
                pos.getY() + 0.5, pos.getZ() + 0.5)) <= (double) maxDistance * maxDistance) {
                return true;
            }
        }
        return false;
    }
    public List<ServerPlayer> getPlayersCloseForSpawning(ChunkPos pos) {
        List<ServerPlayer> close = new java.util.ArrayList<>();
        double cx = pos.getMiddleBlockX();
        double cz = pos.getMiddleBlockZ();
        for (org.bukkit.entity.Player player : ((ServerLevel) source.getLevel()).getWorld().getPlayers()) {
            double dx = player.getLocation().getX() - cx;
            double dz = player.getLocation().getZ() - cz;
            if (player.getGameMode() != org.bukkit.GameMode.SPECTATOR && dx * dx + dz * dz < 16384.0) {
                close.add(((org.bukkit.craftbukkit.entity.CraftPlayer) player).getHandle());
            }
        }
        return close;
    }
    public void move(ServerPlayer player) {
        return;
    }
    public boolean hasEntityWithId(int id) {
        return false;
    }
    protected void addEntity(Entity entity) {
        
    }
    protected void removeEntity(Entity entity) {
        
    }
    protected void tick() {
        
    }
    public void sendToTrackingPlayers(Entity entity, Packet<? super ClientGamePacketListener> packet) {
        for (ServerPlayer player : tracking(entity)) {
            if (player != entity) {
                player.connection.send(packet);
            }
        }
    }
    public void sendToTrackingPlayersFiltered(Entity entity, Packet<? super ClientGamePacketListener> packet, Predicate<ServerPlayer> targetPredicate) {
        for (ServerPlayer player : tracking(entity)) {
            if (player != entity && targetPredicate.test(player)) {
                player.connection.send(packet);
            }
        }
    }
    protected void sendToTrackingPlayersAndSelf(Entity entity, Packet<? super ClientGamePacketListener> packet) {
        for (ServerPlayer player : tracking(entity)) {
            player.connection.send(packet);
        }
    }
    public boolean isTrackedByAnyPlayer(Entity entity) {
        return !tracking(entity).isEmpty();
    }
    public void forEachEntityTrackedBy(ServerPlayer player, Consumer<Entity> consumer) {
        double range = Math.min(128.0, ((ServerLevel) source.getLevel()).getWorld().getViewDistance() * 16.0);
        for (org.bukkit.entity.Entity found : ((org.bukkit.entity.Player) player.getBukkitEntity()).getNearbyEntities(range, range, range)) {
            consumer.accept(((org.bukkit.craftbukkit.entity.CraftEntity) found).getHandle());
        }
    }
    protected PoiManager getPoiManager() {
        return ((ServerLevel) source.getLevel()).getPoiManager();
    }
    void onFullChunkStatusChange(ChunkPos pos, FullChunkStatus status) {
        return;
    }
    public void waitForLightBeforeSending(ChunkPos centerChunk, int chunkRadius) {
        
    }
    public void forEachReadyToSendChunk(Consumer<LevelChunk> consumer) {
        for (org.bukkit.Chunk chunk : ((ServerLevel) source.getLevel()).getWorld().getLoadedChunks()) {
            LevelChunk handle = source.getChunk(chunk.getX(), chunk.getZ(), false);
            if (handle != null) {
                consumer.accept(handle);
            }
        }
    }
    private DistanceManager distanceManager;

    class DistanceManager extends net.minecraft.server.level.DistanceManager {
        protected DistanceManager(TicketStorage ticketStorage, Executor executor, Executor mainThreadExecutor) {
            super(ticketStorage, executor, mainThreadExecutor);
        }

        protected boolean isChunkToRemove(long node) {
            return false;
        }

        protected ChunkHolder getChunk(long node) {
            return getVisibleChunkIfPresent(node);
        }

        protected ChunkHolder updateChunkScheduling(long node, int level, ChunkHolder chunk, int oldLevel) {
            return chunk;
        }

        void bind(org.bukkit.World world) {
            super.bind(world);
        }
    }
    class TrackedEntity implements ServerEntity.Synchronizer {
        private final Entity entity;
        public TrackedEntity(Entity entity, int range, UpdateInterval updateInterval, boolean trackDelta) {
            this.entity = entity;
        }
        public boolean equals(Object obj) {
            return super.equals(obj);
        }
        public int hashCode() {
            return super.hashCode();
        }
        public void sendToTrackingPlayers(Packet<? super ClientGamePacketListener> packet) {
            source.sendToTrackingPlayers(entity, packet);
        }
        public void sendToTrackingPlayersAndSelf(Packet<? super ClientGamePacketListener> packet) {
            source.sendToTrackingPlayersAndSelf(entity, packet);
        }
        public void sendToTrackingPlayersFiltered(Packet<? super ClientGamePacketListener> packet, Predicate<ServerPlayer> targetPredicate) {
            source.sendToTrackingPlayersFiltered(entity, packet, targetPredicate);
        }
        public void broadcastRemoved() {
            sendToTrackingPlayers(new net.minecraft.network.protocol.game.ClientboundRemoveEntitiesPacket(entity.getId()));
        }
        public void removePlayer(ServerPlayer player) {
            player.connection.send(new net.minecraft.network.protocol.game.ClientboundRemoveEntitiesPacket(entity.getId()));
        }
        public void updatePlayer(ServerPlayer player) {
            return;
        }
        public void updatePlayers(List<ServerPlayer> players) {
            return;
        }
    }
}
