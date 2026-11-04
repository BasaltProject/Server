// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk.storage;

import com.mojang.datafixers.DataFixer;
import com.mojang.serialization.Dynamic;
import java.io.IOException;
import java.nio.file.Path;
import java.util.Optional;
import java.util.concurrent.CompletableFuture;
import java.util.function.Supplier;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.Tag;
import net.minecraft.util.datafix.DataFixTypes;
import net.minecraft.world.level.ChunkPos;
public class SimpleRegionStorage implements AutoCloseable {
    protected SimpleRegionStorage() {
    }
    public SimpleRegionStorage(RegionStorageInfo info, Path folder, DataFixer fixerUpper, boolean syncWrites, DataFixTypes dataFixType) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.storage.SimpleRegionStorage#<init>");
    }
    public boolean isOldChunkAround(ChunkPos pos, int range) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.storage.SimpleRegionStorage#isOldChunkAround");
    }
    public CompletableFuture<Optional<CompoundTag>> read(ChunkPos pos) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.storage.SimpleRegionStorage#read");
    }
    public CompletableFuture<Void> write(ChunkPos pos, CompoundTag value) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.storage.SimpleRegionStorage#write");
    }
    public CompletableFuture<Void> write(ChunkPos pos, Supplier<CompoundTag> supplier) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.storage.SimpleRegionStorage#write");
    }
    public CompoundTag upgradeChunkTag(CompoundTag chunkTag, int defaultVersion, CompoundTag dataFixContextTag, int targetVersion) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.storage.SimpleRegionStorage#upgradeChunkTag");
    }
    public CompoundTag upgradeChunkTag(CompoundTag chunkTag, int defaultVersion) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.storage.SimpleRegionStorage#upgradeChunkTag");
    }
    public Dynamic<Tag> upgradeChunkTag(Dynamic<Tag> chunkTag, int defaultVersion) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.storage.SimpleRegionStorage#upgradeChunkTag");
    }
    public static void injectDatafixingContext(CompoundTag chunkTag, CompoundTag contextTag) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.storage.SimpleRegionStorage#injectDatafixingContext");
    }
    public CompletableFuture<Void> synchronize(boolean flush) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.storage.SimpleRegionStorage#synchronize");
    }
    public void close() throws IOException {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.storage.SimpleRegionStorage#close");
    }
    public ChunkScanAccess chunkScanner() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.storage.SimpleRegionStorage#chunkScanner");
    }
    public RegionStorageInfo storageInfo() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.storage.SimpleRegionStorage#storageInfo");
    }
}
