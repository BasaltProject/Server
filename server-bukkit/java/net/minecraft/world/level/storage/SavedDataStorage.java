// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage;

import com.mojang.datafixers.DataFixer;
import java.nio.file.Path;
import java.util.Map;
import net.minecraft.core.HolderLookup;
import java.io.IOException;
import java.nio.file.Files;
import java.util.concurrent.ConcurrentHashMap;
import net.minecraft.nbt.NbtAccounter;
import net.minecraft.nbt.NbtIo;
import net.minecraft.nbt.NbtOps;
import net.minecraft.resources.Identifier;
import java.util.concurrent.CompletableFuture;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.util.datafix.DataFixTypes;
import net.minecraft.world.level.saveddata.SavedData;
import net.minecraft.world.level.saveddata.SavedDataType;
public class SavedDataStorage implements AutoCloseable {
    public Map cache = new ConcurrentHashMap<Identifier, SavedData>();
    private final Map<Identifier, SavedDataType<?>> types = new ConcurrentHashMap<>();
    private final Path directory;
    private final HolderLookup.Provider provider;
    public SavedDataStorage(Path path, DataFixer dataFixer, HolderLookup.Provider provider) {
        this.directory = path;
        this.provider = provider;
    }
    private Path fileFor(SavedDataType<?> type) {
        String key = type.id().getNamespace().equals("minecraft") ? type.id().getPath()
            : type.id().getNamespace() + "_" + type.id().getPath();
        return directory.resolve(key.replace('/', '_') + ".dat");
    }
    @SuppressWarnings("unchecked")
    public <T extends SavedData> T computeIfAbsent(SavedDataType<T> type) {
        T existing = get(type);
        if (existing != null) {
            return existing;
        }
        T fresh = type.constructor().get();
        set(type, fresh);
        return fresh;
    }
    @SuppressWarnings("unchecked")
    public <T extends SavedData> T get(SavedDataType<T> type) {
        Map<Identifier, SavedData> loaded = cache;
        SavedData held = loaded.get(type.id());
        if (held == null) {
            held = readFromDisk(type);
            if (held != null) {
                loaded.put(type.id(), held);
                types.put(type.id(), type);
            }
        }
        return (T) held;
    }
    private <T extends SavedData> T readFromDisk(SavedDataType<T> type) {
        Path file = fileFor(type);
        if (!Files.isRegularFile(file)) {
            return null;
        }
        try {
            CompoundTag tag = readTagFromDisk(file, type.dataFixType(), 0);
            var ops = provider == null ? NbtOps.INSTANCE : provider.createSerializationContext(NbtOps.INSTANCE);
            return type.codec().parse(ops, tag.getCompoundOrEmpty("data")).result().orElse(null);
        } catch (IOException ex) {
            return null;
        }
    }
    @SuppressWarnings("unchecked")
    public <T extends SavedData> void set(SavedDataType<T> type, T data) {
        ((Map<Identifier, SavedData>) cache).put(type.id(), data);
        types.put(type.id(), type);
        data.setDirty();
    }
    public CompoundTag readTagFromDisk(Path dataFile, DataFixTypes type, int newVersion) throws IOException {
        return NbtIo.readCompressed(dataFile, NbtAccounter.create(NbtAccounter.UNCOMPRESSED_NBT_QUOTA));
    }
    @SuppressWarnings({"unchecked", "rawtypes"})
    public CompletableFuture<?> scheduleSave() {
        try {
            Files.createDirectories(directory);
        } catch (IOException ex) {
            return CompletableFuture.failedFuture(ex);
        }
        var ops = provider == null ? NbtOps.INSTANCE : provider.createSerializationContext(NbtOps.INSTANCE);
        for (Map.Entry<Identifier, SavedData> entry : ((Map<Identifier, SavedData>) cache).entrySet()) {
            SavedData data = entry.getValue();
            if (!data.isDirty()) {
                continue;
            }
            SavedDataType type = types.get(entry.getKey());
            var encoded = type.codec().encodeStart(ops, data);
            if (encoded.result().isEmpty()) {
                continue;
            }
            CompoundTag root = new CompoundTag();
            root.put("data", (net.minecraft.nbt.Tag) encoded.result().get());
            try {
                NbtIo.writeCompressed(root, fileFor(type));
                data.setDirty(false);
            } catch (IOException ex) {
                return CompletableFuture.failedFuture(ex);
            }
        }
        return CompletableFuture.completedFuture(null);
    }
    public void saveAndJoin() {
        scheduleSave().join();
    }
    public void close() {
        saveAndJoin();
    }
}
