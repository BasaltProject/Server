// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.debug;

import java.util.Set;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.ai.village.poi.PoiRecord;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.chunk.LevelChunk;

public class LevelDebugSynchronizers {
    private final ServerLevel level;
    private Set<DebugSubscription<?>> enabled = Set.of();

    public LevelDebugSynchronizers(ServerLevel level) {
        this.level = level;
    }

    public ServerLevel level() {
        return level;
    }

    public void tick(ServerDebugSubscribers serverSubscribers) {
        enabled = serverSubscribers.enabledSubscriptions();
    }

    public void registerChunk(LevelChunk chunk) {}

    public void dropChunk(ChunkPos chunkPos) {}

    public void registerBlockEntity(BlockEntity blockEntity) {}

    public void dropBlockEntity(BlockPos blockPos) {}

    public void registerEntity(Entity entity) {}

    public void dropEntity(Entity entity) {}

    public void startTrackingChunk(ServerPlayer player, ChunkPos chunkPos) {}

    public void startTrackingEntity(ServerPlayer player, Entity entity) {}

    public void registerPoi(PoiRecord poi) {}

    public void updatePoi(BlockPos pos) {}

    public void dropPoi(BlockPos pos) {}

    public boolean hasAnySubscriberFor(DebugSubscription<?> subscription) {
        return enabled.contains(subscription);
    }

    public <T> void sendBlockValue(BlockPos blockPos, DebugSubscription<T> subscription, T value) {}

    public <T> void clearBlockValue(BlockPos blockPos, DebugSubscription<T> subscription) {}

    public <T> void sendEntityValue(Entity entity, DebugSubscription<T> subscription, T value) {}

    public <T> void clearEntityValue(Entity entity, DebugSubscription<T> subscription) {}

    public <T> void broadcastEventToTracking(BlockPos blockPos, DebugSubscription<T> subscription, T value) {}
}
