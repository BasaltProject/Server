package net.minecraft.nbt;

import java.io.DataOutput;
import java.io.IOException;
import java.util.Arrays;

public final class LongArrayTag implements CollectionTag {
    private long[] data;

    public LongArrayTag(long[] data) {
        this.data = data;
    }

    public long[] getAsLongArray() {
        return data;
    }

    @Override
    public byte getId() {
        return TAG_LONG_ARRAY;
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
        for (long value : data) {
            output.writeLong(value);
        }
    }

    @Override
    public LongArrayTag copy() {
        return new LongArrayTag(data.clone());
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof LongArrayTag tag && Arrays.equals(tag.data, data);
    }

    @Override
    public int hashCode() {
        return Arrays.hashCode(data);
    }

    @Override
    public String toString() {
        return "[L; " + data.length + " values]";
    }
}
