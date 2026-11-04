// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import java.util.function.IntConsumer;

public interface BitStorage {
    int getAndSet(int index, int value);

    void set(int index, int value);

    int get(int index);

    long[] getRaw();

    int getSize();

    int getBits();

    void getAll(IntConsumer output);

    void unpack(int[] output);

    BitStorage copy();
    default void fill(int value) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.util.BitStorage#fill");
    }
}
