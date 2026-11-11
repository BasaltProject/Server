// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk.storage;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.core.SectionPos;
import com.mojang.serialization.Dynamic;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DynamicOps;
import it.unimi.dsi.fastutil.ints.Int2ObjectMap;
import java.io.IOException;
import java.util.Optional;
import java.util.concurrent.CompletableFuture;
import java.util.function.BiFunction;
import java.util.function.BooleanSupplier;
import java.util.function.Function;
import net.minecraft.core.RegistryAccess;
import net.minecraft.nbt.Tag;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.LevelHeightAccessor;
public class SectionStorage<R, P> implements AutoCloseable {
    private final SimpleRegionStorage regions;
    private final Codec<P> codec;
    private final Function<R, P> packer;
    private final BiFunction<P, Runnable, R> unpacker;
    private final Function<Runnable, R> factory;
    private final RegistryAccess registryAccess;
    private final ChunkIOErrorReporter errorReporter;
    private final it.unimi.dsi.fastutil.longs.Long2ObjectMap<Optional<R>> cells = new it.unimi.dsi.fastutil.longs.Long2ObjectOpenHashMap<>();
    private final it.unimi.dsi.fastutil.longs.LongLinkedOpenHashSet dirty = new it.unimi.dsi.fastutil.longs.LongLinkedOpenHashSet();
    private final it.unimi.dsi.fastutil.longs.LongSet loaded = new it.unimi.dsi.fastutil.longs.LongOpenHashSet();
    protected final LevelHeightAccessor levelHeightAccessor;
    public SectionStorage(SimpleRegionStorage simpleRegionStorage, Codec<P> codec, Function<R, P> packer, BiFunction<P, Runnable, R> unpacker, Function<Runnable, R> factory, RegistryAccess registryAccess, ChunkIOErrorReporter errorReporter, LevelHeightAccessor levelHeightAccessor) {
        this.regions = simpleRegionStorage;
        this.codec = codec;
        this.packer = packer;
        this.unpacker = unpacker;
        this.factory = factory;
        this.registryAccess = registryAccess;
        this.errorReporter = errorReporter;
        this.levelHeightAccessor = levelHeightAccessor;
    }
    private static long cellKey(ChunkPos chunk, int sectionY) {
        return SectionPos.asLong(chunk.x(), sectionY, chunk.z());
    }
    private void ingest(ChunkPos chunk) {
        if (!loaded.add(chunk.pack())) {
            return;
        }
        CompoundTag root = regions.read(chunk).join().orElse(null);
        CompoundTag sections = root == null ? null : regions.upgradeChunkTag(root, 1945).getCompoundOrEmpty("Sections");
        net.minecraft.resources.RegistryOps<net.minecraft.nbt.Tag> ops = registryAccess.createSerializationContext(net.minecraft.nbt.NbtOps.INSTANCE);
        for (int y = levelHeightAccessor.getMinSectionY(); y <= levelHeightAccessor.getMaxSectionY(); y++) {
            long key = cellKey(chunk, y);
            net.minecraft.nbt.Tag raw = sections == null ? null : sections.get(Integer.toString(y));
            Optional<R> cell = raw == null ? Optional.empty()
                : codec.parse(ops, raw).resultOrPartial(message -> {}).map(packed -> unpacker.apply(packed, () -> setDirty(key)));
            cells.put(key, cell);
            if (cell.isPresent()) {
                onSectionLoad(key);
            }
        }
    }
    private void persist(ChunkPos chunk) {
        net.minecraft.resources.RegistryOps<net.minecraft.nbt.Tag> ops = registryAccess.createSerializationContext(net.minecraft.nbt.NbtOps.INSTANCE);
        CompoundTag sections = new CompoundTag();
        for (int y = levelHeightAccessor.getMinSectionY(); y <= levelHeightAccessor.getMaxSectionY(); y++) {
            int each = y;
            Optional<R> cell = cells.get(cellKey(chunk, y));
            if (cell != null && cell.isPresent()) {
                codec.encodeStart(ops, packer.apply(cell.get())).resultOrPartial(message -> {}).ifPresent(tag -> sections.put(Integer.toString(each), tag));
            }
        }
        CompoundTag root = new CompoundTag();
        root.put("Sections", sections);
        root.putInt("DataVersion", net.minecraft.SharedConstants.getCurrentVersion().dataVersion().version());
        regions.write(chunk, root).exceptionally(failure -> {
            errorReporter.reportChunkSaveFailure(failure, regions.storageInfo(), chunk);
            return null;
        });
    }
    protected void tick(BooleanSupplier haveTime) {
        it.unimi.dsi.fastutil.longs.LongIterator each = dirty.iterator();
        while (each.hasNext() && haveTime.getAsBoolean()) {
            ChunkPos chunk = ChunkPos.unpack(each.nextLong());
            each.remove();
            persist(chunk);
        }
    }
    public void flushAll() {
        for (long packed : new it.unimi.dsi.fastutil.longs.LongArrayList(dirty)) {
            persist(ChunkPos.unpack(packed));
        }
        dirty.clear();
    }
    public boolean hasWork() {
        return !dirty.isEmpty();
    }
    protected Optional<R> get(long sectionPos) {
        return cells.get(sectionPos);
    }
    protected Optional<R> getOrLoad(long sectionPos) {
        if (outsideStoredRange(sectionPos)) {
            return Optional.empty();
        }
        Optional<R> known = get(sectionPos);
        if (known == null) {
            ingest(SectionPos.of(sectionPos).chunk());
            known = get(sectionPos);
        }
        return known == null ? Optional.empty() : known;
    }
    protected boolean outsideStoredRange(long sectionPos) {
        return levelHeightAccessor.isOutsideBuildHeight(SectionPos.sectionToBlockCoord(SectionPos.y(sectionPos)));
    }
    protected R getOrCreate(long sectionPos) {
        if (outsideStoredRange(sectionPos)) {
            throw new IllegalArgumentException("sectionPos out of bounds");
        }
        Optional<R> known = getOrLoad(sectionPos);
        if (known.isPresent()) {
            return known.get();
        }
        R created = factory.apply(() -> setDirty(sectionPos));
        cells.put(sectionPos, Optional.of(created));
        return created;
    }
    public CompletableFuture<?> prefetch(ChunkPos chunkPos) {
        ingest(chunkPos);
        return CompletableFuture.completedFuture(null);
    }
    protected void onSectionLoad(long sectionPos) {
    }
    protected void setDirty(long sectionPos) {
        Optional<R> held = cells.get(sectionPos);
        if (held != null && held.isPresent()) {
            dirty.add(ChunkPos.pack(SectionPos.x(sectionPos), SectionPos.z(sectionPos)));
        }
    }
    public void flush(ChunkPos chunkPos) {
        if (dirty.remove(chunkPos.pack())) {
            persist(chunkPos);
        }
    }
    public void close() throws IOException {
        regions.close();
    }
    static record PackedChunk<T>(Int2ObjectMap<T> sectionsByY, boolean versionChanged) {
        public static <T> PackedChunk<T> parse(Codec<T> codec, DynamicOps<Tag> ops, Tag tag, SimpleRegionStorage simpleRegionStorage, LevelHeightAccessor levelHeightAccessor) {
            Int2ObjectMap<T> found = new it.unimi.dsi.fastutil.ints.Int2ObjectOpenHashMap<>();
            Dynamic<Tag> fixed = simpleRegionStorage.upgradeChunkTag(new Dynamic<Tag>(ops, tag), 1945);
            for (int y = levelHeightAccessor.getMinSectionY(); y <= levelHeightAccessor.getMaxSectionY(); y++) {
                int each = y;
                fixed.get("Sections").get(Integer.toString(y)).result().flatMap(data -> codec.parse(data).resultOrPartial(message -> {}))
                    .ifPresent(value -> found.put(each, value));
            }
            return new PackedChunk<T>(found, fixed != null);


        }
    }
}
