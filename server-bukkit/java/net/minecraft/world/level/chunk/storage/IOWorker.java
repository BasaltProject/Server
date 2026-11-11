// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk.storage;
import java.io.IOException;
import java.nio.file.Path;
import java.util.Optional;
import java.util.concurrent.CompletableFuture;
import java.util.function.Supplier;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.StreamTagVisitor;
import net.minecraft.world.level.ChunkPos;
public class IOWorker implements AutoCloseable, ChunkScanAccess {
    public static final Supplier<CompoundTag> STORE_EMPTY = () -> null;
    private final RegionStorageInfo info;
    private final RegionFileStorage storage;
    protected IOWorker(RegionStorageInfo info, Path dir, boolean sync) {
        this.info = info;
        this.storage = new RegionFileStorage(info, dir, sync);
    }
    public boolean isOldChunkAround(ChunkPos pos, int range) {
        return false;
    }
    public CompletableFuture<Void> store(ChunkPos pos, CompoundTag value) {
        return store(pos, () -> value);
    }
    public CompletableFuture<Void> store(ChunkPos pos, Supplier<CompoundTag> supplier) {
        try {
            storage.write(pos, supplier.get());
            return CompletableFuture.completedFuture(null);
        } catch (IOException failure) {
            return CompletableFuture.failedFuture(failure);
        }
    }
    public CompletableFuture<Optional<CompoundTag>> loadAsync(ChunkPos pos) {
        try {
            return CompletableFuture.completedFuture(Optional.ofNullable(storage.read(pos)));
        } catch (IOException failure) {
            return CompletableFuture.failedFuture(failure);
        }
    }
    public CompletableFuture<Void> synchronize(boolean flush) {
        try {
            if (flush) {
                storage.flush();
            }
            return CompletableFuture.completedFuture(null);
        } catch (IOException failure) {
            return CompletableFuture.failedFuture(failure);
        }
    }
    public CompletableFuture<Void> scanChunk(ChunkPos pos, StreamTagVisitor visitor) {
        try {
            storage.scanChunk(pos, visitor);
            return CompletableFuture.completedFuture(null);
        } catch (IOException failure) {
            return CompletableFuture.failedFuture(failure);
        }
    }
    public void close() throws IOException {
        storage.close();
    }
    public RegionStorageInfo storageInfo() {
        return info;
    }
    static enum Priority {
        FOREGROUND, BACKGROUND, SHUTDOWN;
    }
    static class PendingStore {
        final CompoundTag data;
        final CompletableFuture<Void> result = new CompletableFuture<>();
        public PendingStore(CompoundTag data) {
            this.data = data;
        }
    }
    interface ThrowingSupplier<T> {
        T get() throws Exception;
    }
}
