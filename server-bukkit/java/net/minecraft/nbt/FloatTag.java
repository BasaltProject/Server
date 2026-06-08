package net.minecraft.nbt;

import java.io.DataOutput;
import java.io.IOException;

public final class FloatTag implements NumericTag {
    public static final FloatTag ZERO = new FloatTag((float) 0);
    private final float value;

    private FloatTag(float value) {
        this.value = value;
    }

    public static FloatTag valueOf(float value) {
        return new FloatTag(value);
    }

    public float value() {
        return value;
    }

    @Override
    public byte getId() {
        return TAG_FLOAT;
    }

    @Override
    public void write(DataOutput output) throws IOException {
        output.writeFloat(value);
    }

    @Override
    public FloatTag copy() {
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
        return other instanceof FloatTag tag && Float.compare(tag.value, value) == 0;
    }

    @Override
    public int hashCode() {
        return Float.hashCode(value);
    }

    @Override
    public String toString() {
        return value + "f";
    }
}
