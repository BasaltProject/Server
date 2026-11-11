// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk.storage;
import com.mojang.datafixers.DataFixer;
import java.io.IOException;
import java.nio.file.Path;
import java.util.concurrent.CompletableFuture;
import java.util.function.Supplier;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.util.datafix.DataFixTypes;
import net.minecraft.world.level.ChunkPos;
public class RecreatingSimpleRegionStorage extends SimpleRegionStorage {
    private final IOWorker writeWorker;
    private final Path writeFolder;
    public RecreatingSimpleRegionStorage(RegionStorageInfo readInfo, Path readFolder, RegionStorageInfo writeInfo, Path writeFolder, DataFixer fixerUpper, boolean syncWrites, DataFixTypes dataFixType) {
        super(readInfo, readFolder, fixerUpper, syncWrites, dataFixType);
        this.writeFolder = writeFolder;
        this.writeWorker = new IOWorker(writeInfo, writeFolder, syncWrites);
    }
    private static void deleteTree(java.io.File root) {
        java.io.File[] children = root.listFiles();
        if (children != null) {
            for (java.io.File child : children) {
                deleteTree(child);
            }
        }
        root.delete();
    }
    public CompletableFuture<Void> write(ChunkPos pos, Supplier<CompoundTag> supplier) {
        return writeWorker.store(pos, supplier);
    }
    public void close() throws IOException {
        super.close();
        writeWorker.close();
        java.io.File leftover = writeFolder.toFile();
        if (leftover.exists()) {
            deleteTree(leftover);
        }
    }
}
