// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk;

import java.util.List;
import java.util.function.Predicate;
import net.minecraft.core.IdMap;
import net.minecraft.network.FriendlyByteBuf;

public interface Palette<T> {
    int idFor(T value, PaletteResize<T> resizeHandler);

    boolean maybeHas(Predicate<T> predicate);

    T valueFor(int index);

    int getSize();

    Palette<T> copy();

    static int varIntSize(int value) {
        int size = 1;
        while ((value & -128) != 0) {
            size++;
            value >>>= 7;
        }
        return size;
    }

    interface Factory {
        <A> Palette<A> create(int bits, List<A> paletteEntries);
    }
    default void read(FriendlyByteBuf buffer, IdMap<T> globalMap) {

    }
    default void write(FriendlyByteBuf buffer, IdMap<T> globalMap) {

    }
    default int getSerializedSize(IdMap<T> globalMap) {
        return 0;
    }
}
