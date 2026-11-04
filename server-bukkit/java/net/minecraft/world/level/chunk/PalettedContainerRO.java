// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk;

import com.mojang.serialization.DataResult;
import java.util.List;
import java.util.Optional;
import java.util.function.Consumer;
import java.util.function.Predicate;
import java.util.stream.LongStream;
import net.minecraft.network.FriendlyByteBuf;

public interface PalettedContainerRO<T> {
    T get(int x, int y, int z);

    void getAll(Consumer<T> consumer);

    int getSerializedSize();

    int bitsPerEntry();

    boolean maybeHas(Predicate<T> predicate);

    PalettedContainerRO<T> copy();

    PalettedContainer<T> recreate();

    void count(PalettedContainer.CountConsumer<T> output);

    PackedData<T> pack(Strategy<T> strategy);

    interface Unpacker<T, C extends PalettedContainerRO<T>> {
        DataResult<C> read(Strategy<T> strategy, PackedData<T> discData);
    }

    record PackedData<T>(List<T> paletteEntries, Optional<LongStream> storage, int bitsPerEntry) {
        public PackedData(List<T> paletteEntries, Optional<LongStream> storage) {
            this(paletteEntries, storage, -1);
        }
            public static final int UNKNOWN_BITS_PER_ENTRY = -1;
}
    default void write(FriendlyByteBuf buffer) {
        if (this instanceof PalettedContainer<T> container) {
            container.write(buffer);
        } else {
            throw new UnsupportedOperationException("Only paletted containers are written");
        }
    }
    default void forEachInPalette(Consumer<T> consumer) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.PalettedContainerRO#forEachInPalette");
    }
}
