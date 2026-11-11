// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.debug;
import java.util.UUID;
import net.minecraft.core.BlockPos;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.game.ClientGamePacketListener;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.util.Unit;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.ai.village.poi.PoiRecord;
import net.minecraft.world.level.ChunkPos;
public abstract class TrackingDebugSynchronizer<T> {
    protected final DebugSubscription<T> subscription;
    public TrackingDebugSynchronizer(DebugSubscription<T> subscription) {
        this.subscription = subscription;
    }
    public final void tick(ServerLevel level) {
    }
    protected final void sendToPlayersTrackingChunk(ServerLevel level, ChunkPos trackedChunk, Packet<? super ClientGamePacketListener> packet) {
    }
    protected final void sendToPlayersTrackingEntity(ServerLevel level, Entity trackedEntity, Packet<? super ClientGamePacketListener> packet) {
    }
    public final void startTrackingChunk(ServerPlayer player, ChunkPos chunkPos) {
    }
    public final void startTrackingEntity(ServerPlayer player, Entity entity) {
    }
    protected void clear() {
    }
    protected void pollAndSendUpdates(ServerLevel level) {
    }
    protected void sendInitialChunk(ServerPlayer player, ChunkPos chunkPos) {
    }
    protected void sendInitialEntity(ServerPlayer player, Entity entity) {
    }
    public static class SourceSynchronizer<T> extends TrackingDebugSynchronizer<T> {
        public SourceSynchronizer(DebugSubscription<T> subscription) {
            super(subscription);
        }
        protected void clear() {
        }
        protected void pollAndSendUpdates(ServerLevel level) {
        }
        public void registerChunk(ChunkPos chunkPos, DebugValueSource.ValueGetter<T> getter) {
        }
        public void registerBlockEntity(BlockPos blockPos, DebugValueSource.ValueGetter<T> getter) {
        }
        public void registerEntity(UUID entityId, DebugValueSource.ValueGetter<T> getter) {
        }
        public void dropChunk(ChunkPos chunkPos) {
        }
        public void dropBlockEntity(ServerLevel level, BlockPos blockPos) {
        }
        public void dropEntity(Entity entity) {
        }
        protected void sendInitialChunk(ServerPlayer player, ChunkPos chunkPos) {
        }
        protected void sendInitialEntity(ServerPlayer player, Entity entity) {
        }
    }
    static class ValueSource<T> {
        private ValueSource(DebugValueSource.ValueGetter<T> getter) {
            
        }
        public DebugSubscription. Update<T> pollUpdate(DebugSubscription<T> subscription) {
            return subscription.emptyUpdate();
        }
    }
    public static class PoiSynchronizer extends TrackingDebugSynchronizer<DebugPoiInfo> {
        public PoiSynchronizer() {
            super(DebugSubscriptions.POIS);
        }
        protected void sendInitialChunk(ServerPlayer player, ChunkPos chunkPos) {
        }
        public void onPoiAdded(ServerLevel level, PoiRecord record) {
        }
        public void onPoiRemoved(ServerLevel level, BlockPos poiPos) {
        }
        public void onPoiTicketCountChanged(ServerLevel level, BlockPos poiPos) {
        }
    }
    public static class VillageSectionSynchronizer extends TrackingDebugSynchronizer<Unit> {
        public VillageSectionSynchronizer() {
            super(DebugSubscriptions.VILLAGE_SECTIONS);
        }
        protected void sendInitialChunk(ServerPlayer player, ChunkPos chunkPos) {
        }
        public void onPoiAdded(ServerLevel level, PoiRecord record) {
        }
        public void onPoiRemoved(ServerLevel level, BlockPos poiPos) {
        }
    }
}
