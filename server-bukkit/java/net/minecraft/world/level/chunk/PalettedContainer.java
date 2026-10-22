package net.minecraft.world.level.chunk;

import com.mojang.serialization.DataResult;
import java.util.Arrays;
import java.util.List;
import java.util.Optional;
import java.util.function.Consumer;
import java.util.function.Predicate;
import java.util.stream.LongStream;
import net.minecraft.util.BitStorage;
import net.minecraft.util.SimpleBitStorage;
import net.minecraft.util.ThreadingDetector;
import net.minecraft.util.ZeroBitStorage;

/** The names and shapes of the private members are what the world editing adapters look up by reflection. */
public class PalettedContainer<T> implements PaletteResize<T>, PalettedContainerRO<T> {
    private volatile Data<T> data;
    private final Strategy<T> strategy;
    private final ThreadingDetector threadingDetector = new ThreadingDetector("PalettedContainer");

    public void acquire() {
        threadingDetector.checkAndLock();
    }

    public void release() {
        threadingDetector.checkAndUnlock();
    }

    private PalettedContainer(Strategy<T> strategy, Configuration configuration, BitStorage storage, Palette<T> palette) {
        this.strategy = strategy;
        this.data = new Data<>(configuration, storage, palette);
    }

    private PalettedContainer(PalettedContainer<T> source) {
        this.strategy = source.strategy;
        this.data = source.data.copy();
    }

    public PalettedContainer(T initialValue, Strategy<T> strategy) {
        this.strategy = strategy;
        this.data = createOrReuseData(null, 0);
        this.data.palette.idFor(initialValue, this);
    }

    private Data<T> createOrReuseData(Data<T> oldData, int targetBits) {
        Configuration configuration = strategy.getConfigurationForBitCount(targetBits);
        if (oldData != null && configuration.equals(oldData.configuration())) {
            return oldData;
        }
        BitStorage storage = configuration.bitsInMemory() == 0 ? new ZeroBitStorage(strategy.entryCount())
            : new SimpleBitStorage(configuration.bitsInMemory(), strategy.entryCount());
        return new Data<>(configuration, storage, configuration.createPalette(strategy, List.of()));
    }

    @Override
    public int onResize(int bits, T lastAddedValue) {
        Data<T> old = data;
        Data<T> next = createOrReuseData(old, bits);
        next.copyFrom(old.palette, old.storage);
        data = next;
        return next.palette.idFor(lastAddedValue, PaletteResize.noResizeExpected());
    }

    public T getAndSet(int x, int y, int z, T value) {
        acquire();
        try {
            return getAndSet(strategy.getIndex(x, y, z), value);
        } finally {
            release();
        }
    }

    public T getAndSetUnchecked(int x, int y, int z, T value) {
        return getAndSet(strategy.getIndex(x, y, z), value);
    }

    private T getAndSet(int index, T value) {
        Data<T> held = data;
        int id = held.palette.idFor(value, this);
        int oldId = data.storage.getAndSet(index, id);
        return data.palette.valueFor(oldId);
    }

    public void set(int x, int y, int z, T value) {
        acquire();
        try {
            int id = data.palette.idFor(value, this);
            data.storage.set(strategy.getIndex(x, y, z), id);
        } finally {
            release();
        }
    }

    @Override
    public T get(int x, int y, int z) {
        return get(strategy.getIndex(x, y, z));
    }

    protected T get(int index) {
        Data<T> held = data;
        return held.palette.valueFor(held.storage.get(index));
    }

    @Override
    public void getAll(Consumer<T> consumer) {
        Data<T> held = data;
        java.util.BitSet seen = new java.util.BitSet();
        held.storage.getAll(seen::set);
        for (int id = seen.nextSetBit(0); id >= 0; id = seen.nextSetBit(id + 1)) {
            consumer.accept(held.palette.valueFor(id));
        }
    }

    public static <T> DataResult<PalettedContainer<T>> unpack(Strategy<T> strategy, PackedData<T> discData) {
        List<T> entries = discData.paletteEntries();
        int entryCount = strategy.entryCount();
        Configuration stored = strategy.getConfigurationForPaletteSize(entries.size());
        int bitsOnDisc = stored.bitsInStorage();
        if (discData.bitsPerEntry() != -1 && bitsOnDisc != discData.bitsPerEntry()) {
            return DataResult.error(() -> "Invalid bit count, calculated " + bitsOnDisc + ", but container declared "
                + discData.bitsPerEntry());
        }
        BitStorage storage;
        Palette<T> palette;
        if (stored.bitsInMemory() == 0) {
            palette = stored.createPalette(strategy, entries);
            storage = new ZeroBitStorage(entryCount);
        } else {
            Optional<LongStream> source = discData.storage();
            if (source.isEmpty()) {
                return DataResult.error(() -> "Missing values for non-zero storage");
            }
            long[] raw = source.get().toArray();
            try {
                if (!stored.alwaysRepack() && stored.bitsInMemory() == bitsOnDisc) {
                    palette = stored.createPalette(strategy, entries);
                    storage = new SimpleBitStorage(stored.bitsInMemory(), entryCount, raw);
                } else {
                    Palette<T> oldPalette = new HashMapPalette<>(bitsOnDisc, entries);
                    SimpleBitStorage oldStorage = new SimpleBitStorage(bitsOnDisc, entryCount, raw);
                    Palette<T> newPalette = stored.createPalette(strategy, entries);
                    int[] contents = reencodeContents(oldStorage, oldPalette, newPalette);
                    palette = newPalette;
                    storage = new SimpleBitStorage(stored.bitsInMemory(), entryCount, contents);
                }
            } catch (SimpleBitStorage.InitializationException e) {
                return DataResult.error(() -> "Failed to read PalettedContainer: " + e.getMessage());
            }
        }
        return DataResult.success(new PalettedContainer<>(strategy, stored, storage, palette));
    }

    /** The name the world editing adapters look up when they are not run on a Paper server. */
    public static <T> DataResult<PalettedContainer<T>> a(Strategy<T> strategy, PackedData<T> discData) {
        return unpack(strategy, discData);
    }

    public static <T> DataResult<PalettedContainer<T>> unpack(Strategy<T> strategy, PackedData<T> discData,
                                                              T defaultValue, T[] unused) {
        return unpack(strategy, discData);
    }

    @Override
    public PackedData<T> pack(Strategy<T> strategy) {
        acquire();
        try {
            BitStorage currentStorage = data.storage;
            Palette<T> currentPalette = data.palette;
            HashMapPalette<T> newPalette = new HashMapPalette<>(currentStorage.getBits());
            int[] contents = reencodeContents(currentStorage, currentPalette, newPalette);
            Configuration stored = strategy.getConfigurationForPaletteSize(newPalette.getSize());
            int bitsOnDisc = stored.bitsInStorage();
            Optional<LongStream> values = bitsOnDisc == 0 ? Optional.empty()
                : Optional.of(Arrays.stream(new SimpleBitStorage(bitsOnDisc, strategy.entryCount(), contents).getRaw()));
            return new PackedData<>(newPalette.getEntries(), values, bitsOnDisc);
        } finally {
            release();
        }
    }

    private static <T> int[] reencodeContents(BitStorage storage, Palette<T> oldPalette, Palette<T> newPalette) {
        int[] buffer = new int[storage.getSize()];
        storage.unpack(buffer);
        PaletteResize<T> dummy = PaletteResize.noResizeExpected();
        int lastRead = -1;
        int lastWritten = -1;
        for (int i = 0; i < buffer.length; i++) {
            int id = buffer[i];
            if (id != lastRead) {
                lastRead = id;
                lastWritten = newPalette.idFor(oldPalette.valueFor(id), dummy);
            }
            buffer[i] = lastWritten;
        }
        return buffer;
    }

    @Override
    public int getSerializedSize() {
        return 1 + data.storage.getRaw().length * 8;
    }

    @Override
    public int bitsPerEntry() {
        return data.storage.getBits();
    }

    @Override
    public boolean maybeHas(Predicate<T> predicate) {
        return data.palette.maybeHas(predicate);
    }

    @Override
    public PalettedContainer<T> copy() {
        return new PalettedContainer<>(this);
    }

    @Override
    public PalettedContainer<T> recreate() {
        return new PalettedContainer<>(data.palette.valueFor(0), strategy);
    }

    @Override
    public void count(CountConsumer<T> output) {
        Data<T> held = data;
        if (held.palette.getSize() == 1) {
            output.accept(held.palette.valueFor(0), held.storage.getSize());
            return;
        }
        int[] counts = new int[held.palette.getSize()];
        held.storage.getAll(id -> counts[id]++);
        for (int id = 0; id < counts.length; id++) {
            if (counts[id] > 0) {
                output.accept(held.palette.valueFor(id), counts[id]);
            }
        }
    }

    /** Every entry of the container as state ids or whatever `ids` says, in index order. */
    public int[] toIds(java.util.function.ToIntFunction<T> ids) {
        Data<T> held = data;
        int[] out = new int[strategy.entryCount()];
        if (held.storage instanceof ZeroBitStorage) {
            Arrays.fill(out, ids.applyAsInt(held.palette.valueFor(0)));
            return out;
        }
        held.storage.unpack(out);
        if (held.palette instanceof GlobalPalette) {
            for (int i = 0; i < out.length; i++) {
                out[i] = ids.applyAsInt(held.palette.valueFor(out[i]));
            }
            return out;
        }
        int size = held.palette.getSize();
        int[] mapped = new int[size];
        for (int i = 0; i < size; i++) {
            mapped[i] = ids.applyAsInt(held.palette.valueFor(i));
        }
        for (int i = 0; i < out.length; i++) {
            out[i] = mapped[out[i]];
        }
        return out;
    }

    /** Builds a container holding `ids[i]` mapped through `values` at index i, with the vanilla palette choice. */
    public static <T> PalettedContainer<T> fromIds(Strategy<T> strategy, int[] ids, java.util.function.IntFunction<T> values,
                                                   int idLimit) {
        int count = ids.length;
        int[] local = new int[idLimit];
        Arrays.fill(local, -1);
        int[] order = new int[Math.min(idLimit, count)];
        int distinct = 0;
        int[] indices = new int[count];
        for (int i = 0; i < count; i++) {
            int id = ids[i];
            int at = local[id];
            if (at < 0) {
                at = distinct;
                local[id] = distinct;
                order[distinct++] = id;
            }
            indices[i] = at;
        }
        Configuration stored = strategy.getConfigurationForPaletteSize(distinct);
        if (stored.bitsInMemory() == 0) {
            List<T> one = List.of(values.apply(order[0]));
            return new PalettedContainer<>(strategy, stored, new ZeroBitStorage(count), stored.createPalette(strategy, one));
        }
        if (stored.alwaysRepack()) {
            Palette<T> palette = stored.createPalette(strategy, List.of());
            return new PalettedContainer<>(strategy, stored,
                new SimpleBitStorage(stored.bitsInMemory(), count, ids.clone()), palette);
        }
        List<T> entries = new java.util.ArrayList<>(distinct);
        for (int i = 0; i < distinct; i++) {
            entries.add(values.apply(order[i]));
        }
        return new PalettedContainer<>(strategy, stored,
            new SimpleBitStorage(stored.bitsInMemory(), count, indices), stored.createPalette(strategy, entries));
    }

    private record Data<T>(Configuration configuration, BitStorage storage, Palette<T> palette) {
        void copyFrom(Palette<T> oldPalette, BitStorage oldStorage) {
            PaletteResize<T> dummy = PaletteResize.noResizeExpected();
            if (oldPalette.getSize() == 1) {
                T value = oldPalette.valueFor(0);
                int id = palette.idFor(value, dummy);
                for (int i = 0; i < oldStorage.getSize(); i++) {
                    storage.set(i, id);
                }
                return;
            }
            int lastOld = -1;
            int newId = -1;
            for (int i = 0; i < oldStorage.getSize(); i++) {
                int oldId = oldStorage.get(i);
                if (oldId != lastOld) {
                    newId = palette.idFor(oldPalette.valueFor(oldId), dummy);
                    lastOld = oldId;
                }
                storage.set(i, newId);
            }
        }

        Data<T> copy() {
            return new Data<>(configuration, storage.copy(), palette.copy());
        }
    }

    @FunctionalInterface
    public interface CountConsumer<T> {
        void accept(T entry, int count);
    }
}
