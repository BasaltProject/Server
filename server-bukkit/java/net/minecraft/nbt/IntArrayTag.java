package net.minecraft.nbt;

import java.io.DataOutput;
import java.io.IOException;
import java.util.Arrays;

public final class IntArrayTag implements CollectionTag {
    private int[] data;

    public IntArrayTag(int[] data) {
        this.data = data;
    }

    public int[] getAsIntArray() {
        return data;
    }

    @Override
    public byte getId() {
        return TAG_INT_ARRAY;
    }

    @Override
    public int size() {
        return data.length;
    }

    @Override
    public boolean isEmpty() {
        return data.length == 0;
    }

    @Override
    public void write(DataOutput output) throws IOException {
        output.writeInt(data.length);
        for (int value : data) {
            output.writeInt(value);
        }
    }

    @Override
    public IntArrayTag copy() {
        return new IntArrayTag(data.clone());
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof IntArrayTag tag && Arrays.equals(tag.data, data);
    }

    @Override
    public int hashCode() {
        return Arrays.hashCode(data);
    }

    @Override
    public String toString() {
        return "[I; " + data.length + " values]";
    }
}
