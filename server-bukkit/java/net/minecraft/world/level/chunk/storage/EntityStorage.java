// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk.storage;
import java.util.List;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.ListTag;
import java.io.IOException;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.Executor;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.entity.ChunkEntities;
import net.minecraft.world.level.entity.EntityPersistentStorage;
public class EntityStorage implements EntityPersistentStorage<Entity> {
    private final SimpleRegionStorage regions;
    private final it.unimi.dsi.fastutil.longs.LongSet blankChunks = new it.unimi.dsi.fastutil.longs.LongOpenHashSet();
    public EntityStorage(SimpleRegionStorage simpleRegionStorage, ServerLevel level, Executor mainThreadExecutor) {
        this.regions = simpleRegionStorage;
    }
    public CompletableFuture<ChunkEntities<Entity>> loadEntities(ChunkPos pos) {
        return regions.read(pos).thenApply(stored -> {
            if (stored.isEmpty()) {
                blankChunks.add(pos.pack());
            }
            return new ChunkEntities<Entity>(pos, List.of());
        });
    }
    public void storeEntities(ChunkEntities<Entity> chunk) {
        ChunkPos pos = chunk.getPos();
        if (chunk.isEmpty()) {
            if (blankChunks.add(pos.pack())) {
                regions.write(pos, IOWorker.STORE_EMPTY);
            }
            return;
        }
        CompoundTag marker = new CompoundTag();
        marker.put("Entities", new ListTag());
        marker.putIntArray("Position", new int[] {pos.x(), pos.z()});
        regions.write(pos, marker);
        blankChunks.remove(pos.pack());
    }
    public void flush(boolean flushStorage) {
        regions.synchronize(flushStorage).join();
    }
    public void close() throws IOException {
        regions.close();
    }
}
