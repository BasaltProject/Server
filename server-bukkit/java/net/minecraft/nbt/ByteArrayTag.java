package net.minecraft.nbt;

import java.io.DataOutput;
import java.io.IOException;
import java.util.Arrays;

public final class ByteArrayTag implements CollectionTag {
    private byte[] data;

    public ByteArrayTag(byte[] data) {
        this.data = data;
    }

    public byte[] getAsByteArray() {
        return data;
    }

    @Override
    public byte getId() {
        return TAG_BYTE_ARRAY;
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
        for (byte value : data) {
            output.writeByte(value);
        }
    }

    @Override
    public ByteArrayTag copy() {
        return new ByteArrayTag(data.clone());
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof ByteArrayTag tag && Arrays.equals(tag.data, data);
    }

    @Override
    public int hashCode() {
        return Arrays.hashCode(data);
    }

    @Override
    public String toString() {
        return "[B; " + data.length + " values]";
    }
}
