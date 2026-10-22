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

public class ThreadedLevelLightEngine extends LevelLightEngine {
    private final String world;
    private final Map<DataLayer, Handed> handed = new ConcurrentHashMap<>();

    private record Handed(int layer, int x, int y, int z, int hash) {}

    ThreadedLevelLightEngine(String world) {
        this.world = world;
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
}
