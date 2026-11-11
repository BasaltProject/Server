// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix.access;

import java.io.IOException;
import java.nio.file.Path;
import java.util.concurrent.CompletableFuture;
import java.util.function.UnaryOperator;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.util.Util;
import net.minecraft.util.datafix.DataFixTypes;
import net.minecraft.util.datafix.DataFixers;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.chunk.storage.RegionStorageInfo;
import net.minecraft.world.level.chunk.storage.SimpleRegionStorage;

public class ChunkNbt implements AutoCloseable {
    private final SimpleRegionStorage storage;
    private final int targetVersion;

    public ChunkNbt(RegionStorageInfo info, Path path, DataFixTypes type, int targetVersion) {
        this.targetVersion = targetVersion;
        this.storage = new SimpleRegionStorage(info, path, DataFixers.getDataFixer(), false, type);
    }

    public CompletableFuture<?> updateChunk(ChunkPos pos, CompoundTag dataFixContext, UnaryOperator<CompoundTag> fixer) {
        return storage.read(pos).thenComposeAsync(maybe -> {
            if (maybe.isEmpty()) {
                return CompletableFuture.completedFuture(null);
            }
            CompoundTag tag = storage.upgradeChunkTag(maybe.get(), -1, dataFixContext, targetVersion);
            return storage.write(pos, fixer.apply(tag));
        }, Util.backgroundExecutor());
    }

    @Override
    public void close() throws IOException {
        storage.close();
    }
}
