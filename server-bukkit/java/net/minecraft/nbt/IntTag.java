package net.minecraft.nbt;

import java.io.DataOutput;
import java.io.IOException;

public final class IntTag implements NumericTag {
    public static final IntTag ZERO = new IntTag((int) 0);
    private final int value;

    private IntTag(int value) {
        this.value = value;
    }

    public static IntTag valueOf(int value) {
        return new IntTag(value);
    }

    public int value() {
        return value;
    }

    @Override
    public byte getId() {
        return TAG_INT;
    }

    @Override
    public void write(DataOutput output) throws IOException {
        output.writeInt(value);
    }

    @Override
    public IntTag copy() {
        return this;
    }

    @Override
    public long longValue() {
        return (long) value;
    }

    @Override
    public int intValue() {
        return (int) value;
    }

    @Override
    public short shortValue() {
        return (short) value;
    }

    @Override
    public byte byteValue() {
        return (byte) value;
    }

    @Override
    public double doubleValue() {
        return (double) value;
    }

    @Override
    public float floatValue() {
        return (float) value;
    }

    @Override
    public Number box() {
        return value;
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof IntTag tag && tag.value == value;
    }

    @Override
    public int hashCode() {
        return Long.hashCode((long) value);
    }

    @Override
    public String toString() {
        return value + "";
    }
}
