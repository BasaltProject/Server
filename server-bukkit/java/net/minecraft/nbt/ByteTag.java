// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.nbt;

import java.io.DataOutput;
import java.io.IOException;

public final class ByteTag implements NumericTag {
    public static final ByteTag ZERO = new ByteTag((byte) 0);
    private final byte value;

    private ByteTag(byte value) {
        this.value = value;
    }

    public static ByteTag valueOf(byte value) {
        return new ByteTag(value);
    }

    public byte value() {
        return value;
    }

    @Override
    public byte getId() {
        return TAG_BYTE;
    }

    @Override
    public void write(DataOutput output) throws IOException {
        output.writeByte(value);
    }

    @Override
    public ByteTag copy() {
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
        return other instanceof ByteTag tag && tag.value == value;
    }

    @Override
    public int hashCode() {
        return Long.hashCode((long) value);
    }

    @Override
    public String toString() {
        return value + "b";
    }
    public static final TagType<ByteTag> TYPE = org.basaltmc.bukkit.facade.NbtSupport.type("BYTE", "TAG_Byte",
        (in, acc) -> {
            acc.accountBytes(9);
            return ByteTag.valueOf(in.readByte());
        });
    public static final ByteTag ONE = new ByteTag((byte) 1);
    public static ByteTag valueOf(boolean data) {
        return data ? ONE : ZERO;
    }
    public int sizeInBytes() {
        return 9;
    }
    public TagType<ByteTag> getType() {
        return TYPE;
    }
    public void accept(TagVisitor visitor) {
        visitor.visitByte(this);
    }
    public StreamTagVisitor.ValueResult accept(StreamTagVisitor visitor) {
        return visitor.visit(value);
    }
    static class Cache {
    }
}
