package net.minecraft.util;

import java.util.Arrays;
import java.util.function.IntConsumer;

public class ZeroBitStorage implements BitStorage {
    public static final long[] RAW = new long[0];
    private final int size;

    public ZeroBitStorage(int size) {
        this.size = size;
    }

    @Override
    public int getAndSet(int index, int value) {
        return 0;
    }

    @Override
    public void set(int index, int value) {}

    @Override
    public int get(int index) {
        return 0;
    }

    @Override
    public long[] getRaw() {
        return RAW;
    }

    @Override
    public int getSize() {
        return size;
    }

    @Override
    public int getBits() {
        return 0;
    }

    @Override
    public void getAll(IntConsumer output) {
        for (int i = 0; i < size; i++) {
            output.accept(0);
        }
    }

    @Override
    public void unpack(int[] output) {
        Arrays.fill(output, 0, size, 0);
    }

    @Override
    public BitStorage copy() {
        return this;
    }
}
