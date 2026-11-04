// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

import java.util.Arrays;
import java.util.Collection;
import java.util.Map;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ConcurrentHashMap;
import java.util.function.Consumer;
import java.util.function.IntConsumer;
import net.minecraft.core.BlockPos;
import net.minecraft.core.SectionPos;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.LightLayer;
import net.minecraft.world.level.chunk.DataLayer;
import net.minecraft.world.level.lighting.LayerLightEventListener;
import net.minecraft.world.level.lighting.LevelLightEngine;
import org.basaltmc.bukkit.WorldNatives;
import net.minecraft.util.thread.ConsecutiveExecutor;
import net.minecraft.world.level.chunk.ChunkAccess;
import net.minecraft.world.level.chunk.LightChunkGetter;

public class ThreadedLevelLightEngine extends LevelLightEngine implements AutoCloseable {
    private final String world;
    private final Map<DataLayer, Handed> handed = new ConcurrentHashMap<>();

    private record Handed(int layer, int x, int y, int z, int hash) {}

    private final net.minecraft.world.level.LevelHeightAccessor heights;

    ThreadedLevelLightEngine(String world, net.minecraft.world.level.LevelHeightAccessor heights) {
        this.world = world;
        this.heights = heights;
    }

    @Override
    public int getRawBrightness(net.minecraft.core.BlockPos pos, int skyDampen) {
        flush();
        int block = Math.max(0, WorldNatives.lightAt(world, pos.getX(), pos.getY(), pos.getZ(), 0));
        int sky = Math.max(0, WorldNatives.lightAt(world, pos.getX(), pos.getY(), pos.getZ(), 1));
        return Math.max(block, sky - skyDampen);
    }

    @Override
    public int getMinLightSection() {
        return heights.getMinSectionY() - 1;
    }

    @Override
    public int getMaxLightSection() {
        return heights.getMaxSectionY() + 1;
    }

    @Override
    public int getLightSectionCount() {
        return getMaxLightSection() - getMinLightSection() + 1;
    }

    private static int index(LightLayer layer) {
        return layer == LightLayer.SKY ? 1 : 0;
    }

    public void flush() {
        if (handed.isEmpty()) {
            return;
        }
        for (Map.Entry<DataLayer, Handed> entry : handed.entrySet()) {
            if (handed.remove(entry.getKey(), entry.getValue())) {
                Handed at = entry.getValue();
                byte[] data = entry.getKey().getData();
                int hash;
                synchronized (entry.getKey()) {
                    hash = Arrays.hashCode(data);
                }
                if (hash != at.hash) {
                    WorldNatives.writeLight(world, at.x, at.y, at.z, at.layer, data);
                }
            }
        }
    }

    @Override
    public void checkBlock(BlockPos pos) {
        flush();
        WorldNatives.checkLight(world, new int[] {pos.getX(), pos.getY(), pos.getZ()});
    }

    @Override
    public void queueSectionData(LightLayer layer, SectionPos pos, DataLayer data) {
        flush();
        if (data != null) {
            handed.remove(data);
            WorldNatives.writeLight(world, pos.getX(), pos.getY(), pos.getZ(), index(layer), data.getData());
        }
    }

    public LayerLightEventListener getLayerListener(LightLayer lightLayer) {
        int layer = index(lightLayer);
        return new LayerLightEventListener() {
            @Override
            public DataLayer getDataLayerData(SectionPos pos) {
                flush();
                byte[] bytes = WorldNatives.readLight(world, pos.getX(), pos.getY(), pos.getZ(), layer, false);
                if (bytes == null) {
                    return null;
                }
                DataLayer data = new DataLayer(bytes);
                handed.put(data, new Handed(layer, pos.getX(), pos.getY(), pos.getZ(), Arrays.hashCode(bytes)));
                return data;
            }
        };
    }

    public int starlight$serverRelightChunks(Collection collection, Consumer consumer, IntConsumer intConsumer) {
        flush();
        Object[] chunks = collection.toArray();
        int[] pairs = new int[chunks.length * 2];
        for (int i = 0; i < chunks.length; i++) {
            ChunkPos pos = (ChunkPos) chunks[i];
            pairs[i * 2] = pos.x();
            pairs[i * 2 + 1] = pos.z();
        }
        CompletableFuture<Integer> done = new CompletableFuture<>();
        done.thenAccept(relit -> {
            for (Object chunk : chunks) {
                if (consumer != null) {
                    consumer.accept(chunk);
                }
            }
            if (intConsumer != null) {
                intConsumer.accept(chunks.length);
            }
        });
        WorldNatives.lightJob(world, pairs, done);
        return chunks.length;
    }
    public static final int DEFAULT_BATCH_SIZE = 1000;
    public ThreadedLevelLightEngine(LightChunkGetter lightChunkGetter, ChunkMap chunkMap, boolean hasSkyLight, ConsecutiveExecutor consecutiveExecutor, ChunkTaskDispatcher taskDispatcher) {
        super((net.minecraft.world.level.chunk.LightChunkGetter) null, false, false); throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.level.ThreadedLevelLightEngine#<init>");
    }
    public void close() {
        flush();
    }
    public int runLightUpdates() {
        flush();
        return 0;
    }
    protected void updateChunkStatus(ChunkPos pos) {
        flush();
    }
    public void updateSectionStatus(SectionPos pos, boolean sectionEmpty) {
        flush();
    }
    public void propagateLightSources(ChunkPos pos) {
        flush();
    }
    public void setLightEnabled(ChunkPos pos, boolean enable) {
        return;
    }
    public void retainData(ChunkPos pos, boolean retain) {
        return;
    }
    public CompletableFuture<ChunkAccess> initializeLight(ChunkAccess chunk, boolean lighted) {
        flush();
        return CompletableFuture.completedFuture(chunk);
    }
    public CompletableFuture<ChunkAccess> lightChunk(ChunkAccess centerChunk, boolean lighted) {
        flush();
        return CompletableFuture.completedFuture(centerChunk);
    }
    public void tryScheduleUpdate() {
        flush();
    }
    public CompletableFuture<?> waitForPendingTasks(int chunkX, int chunkZ) {
        flush();
        return CompletableFuture.completedFuture(null);
    }
    static enum TaskType {
        PRE_UPDATE, POST_UPDATE;
    }
}
