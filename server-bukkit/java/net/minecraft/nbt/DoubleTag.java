package net.minecraft.nbt;

import java.io.DataOutput;
import java.io.IOException;

public final class DoubleTag implements NumericTag {
    public static final DoubleTag ZERO = new DoubleTag((double) 0);
    private final double value;

    private DoubleTag(double value) {
        this.value = value;
    }

    public static DoubleTag valueOf(double value) {
        return new DoubleTag(value);
    }

    public double value() {
        return value;
    }

    @Override
    public byte getId() {
        return TAG_DOUBLE;
    }

    @Override
    public void write(DataOutput output) throws IOException {
        output.writeDouble(value);
    }

    @Override
    public DoubleTag copy() {
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
        return other instanceof DoubleTag tag && Double.compare(tag.value, value) == 0;
    }

    @Override
    public int hashCode() {
        return Double.hashCode(value);
    }

    @Override
    public String toString() {
        return value + "d";
    }
}
