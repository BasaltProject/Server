package net.minecraft.nbt;

import java.io.DataOutput;
import java.io.IOException;

public final class StringTag implements PrimitiveTag {
    private final String value;

    private StringTag(String value) {
        this.value = value;
    }

    public static StringTag valueOf(String value) {
        return new StringTag(value);
    }

    public String value() {
        return value;
    }

    @Override
    public byte getId() {
        return TAG_STRING;
    }

    @Override
    public void write(DataOutput output) throws IOException {
        output.writeUTF(value);
    }

    @Override
    public StringTag copy() {
        return this;
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof StringTag tag && tag.value.equals(value);
    }

    @Override
    public int hashCode() {
        return value.hashCode();
    }

    @Override
    public String toString() {
        return "\"" + value.replace("\\", "\\\\").replace("\"", "\\\"") + "\"";
    }
}
