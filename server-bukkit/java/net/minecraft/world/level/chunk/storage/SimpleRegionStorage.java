// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk.storage;

import com.mojang.datafixers.DataFixer;
import com.mojang.serialization.Dynamic;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.UncheckedIOException;
import java.nio.file.Path;
import java.util.Optional;
import java.util.concurrent.CompletableFuture;
import java.util.function.Supplier;
import net.minecraft.SharedConstants;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.NbtAccounter;
import net.minecraft.nbt.NbtIo;
import net.minecraft.nbt.NbtOps;
import net.minecraft.nbt.Tag;
import net.minecraft.util.datafix.DataFixTypes;
import net.minecraft.world.level.ChunkPos;
import org.basaltmc.bukkit.MechanicsNatives;

public class SimpleRegionStorage implements AutoCloseable {
    private static final String kContextKey = "__context";
    private final RegionStorageInfo info;
    private final String folder;
    private final DataFixer fixerUpper;
    private final DataFixTypes dataFixType;

    protected SimpleRegionStorage() {
        this.info = null;
        this.folder = null;
        this.fixerUpper = null;
        this.dataFixType = null;
    }

    public SimpleRegionStorage(RegionStorageInfo info, Path folder, DataFixer fixerUpper, boolean syncWrites, DataFixTypes dataFixType) {
        this.info = info;
        this.folder = folder.toAbsolutePath().toString();
        this.fixerUpper = fixerUpper;
        this.dataFixType = dataFixType;
    }

    public boolean isOldChunkAround(ChunkPos pos, int range) {
        int current = SharedConstants.getCurrentVersion().dataVersion().version();
        for (int dz = -range; dz <= range; dz++) {
            for (int dx = -range; dx <= range; dx++) {
                CompoundTag tag = readNow(new ChunkPos(pos.x() + dx, pos.z() + dz));
                if (tag != null && tag.getIntOr("DataVersion", 0) < current) {
                    return true;
                }
            }
        }
        return false;
    }

    private CompoundTag readNow(ChunkPos pos) {
        byte[] bytes = MechanicsNatives.regionRead(folder, pos.x(), pos.z());
        if (bytes == null) {
            return null;
        }
        try {
            return NbtIo.read(new DataInputStream(new ByteArrayInputStream(bytes)), NbtAccounter.unlimitedHeap());
        } catch (IOException e) {
            throw new UncheckedIOException(e);
        }
    }

    public CompletableFuture<Optional<CompoundTag>> read(ChunkPos pos) {
        try {
            return CompletableFuture.completedFuture(Optional.ofNullable(readNow(pos)));
        } catch (RuntimeException e) {
            return CompletableFuture.failedFuture(e);
        }
    }

    public CompletableFuture<Void> write(ChunkPos pos, CompoundTag value) {
        try {
            if (value == null) {
                MechanicsNatives.regionWrite(folder, pos.x(), pos.z(), null, 0);
            } else {
                ByteArrayOutputStream bytes = new ByteArrayOutputStream();
                NbtIo.write(value, new DataOutputStream(bytes));
                MechanicsNatives.regionWrite(folder, pos.x(), pos.z(), bytes.toByteArray(), (int) (System.currentTimeMillis() / 1000L));
            }
            return CompletableFuture.completedFuture(null);
        } catch (IOException e) {
            return CompletableFuture.failedFuture(e);
        }
    }

    public CompletableFuture<Void> write(ChunkPos pos, Supplier<CompoundTag> supplier) {
        return write(pos, supplier.get());
    }

    public CompoundTag upgradeChunkTag(CompoundTag chunkTag, int defaultVersion, CompoundTag dataFixContextTag, int targetVersion) {
        if (dataFixContextTag != null) {
            injectDatafixingContext(chunkTag, dataFixContextTag);
        }
        CompoundTag upgraded = dataFixType.update(fixerUpper, chunkTag, chunkTag.getIntOr("DataVersion", defaultVersion), targetVersion);
        upgraded.remove(kContextKey);
        return upgraded;
    }

    public CompoundTag upgradeChunkTag(CompoundTag chunkTag, int defaultVersion) {
        return upgradeChunkTag(chunkTag, defaultVersion, null, SharedConstants.getCurrentVersion().dataVersion().version());
    }

    public Dynamic<Tag> upgradeChunkTag(Dynamic<Tag> chunkTag, int defaultVersion) {
        CompoundTag upgraded = upgradeChunkTag((CompoundTag) chunkTag.getValue(), defaultVersion);
        return new Dynamic<>(NbtOps.INSTANCE, upgraded);
    }

    public static void injectDatafixingContext(CompoundTag chunkTag, CompoundTag contextTag) {
        chunkTag.put(kContextKey, contextTag);
    }

    public CompletableFuture<Void> synchronize(boolean flush) {
        if (flush) {
            MechanicsNatives.regionFlush(folder, false);
        }
        return CompletableFuture.completedFuture(null);
    }

    public void close() throws IOException {
        if (!MechanicsNatives.regionFlush(folder, true)) {
            throw new IOException("Could not write the regions in " + folder);
        }
    }

    public ChunkScanAccess chunkScanner() {
        return (pos, visitor) -> {
            try {
                CompoundTag tag = readNow(pos);
                if (tag != null) {
                    tag.accept(visitor);
                }
                return CompletableFuture.completedFuture(null);
            } catch (RuntimeException e) {
                return CompletableFuture.failedFuture(e);
            }
        };
    }

    public RegionStorageInfo storageInfo() {
        return info;
    }
}
