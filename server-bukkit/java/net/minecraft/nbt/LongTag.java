// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.nbt;

import java.io.DataOutput;
import java.io.IOException;

public final class LongTag implements NumericTag {
    public static final LongTag ZERO = new LongTag((long) 0);
    private final long value;

    private LongTag(long value) {
        this.value = value;
    }

    public static LongTag valueOf(long value) {
        return new LongTag(value);
    }

    public long value() {
        return value;
    }

    @Override
    public byte getId() {
        return TAG_LONG;
    }

    @Override
    public void write(DataOutput output) throws IOException {
        output.writeLong(value);
    }

    @Override
    public LongTag copy() {
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
        return other instanceof LongTag tag && tag.value == value;
    }

    @Override
    public int hashCode() {
        return Long.hashCode((long) value);
    }

    @Override
    public String toString() {
        return value + "L";
    }
    public static final TagType<LongTag> TYPE = org.basaltmc.bukkit.facade.NbtSupport.type("LONG", "TAG_Long",
        (in, acc) -> {
            acc.accountBytes(16);
            return LongTag.valueOf(in.readLong());
        });
    public int sizeInBytes() {
        return 16;
    }
    public TagType<LongTag> getType() {
        return TYPE;
    }
    public void accept(TagVisitor visitor) {
        visitor.visitLong(this);
    }
    public StreamTagVisitor.ValueResult accept(StreamTagVisitor visitor) {
        return visitor.visit(value);
    }
    static class Cache {
    }
}
