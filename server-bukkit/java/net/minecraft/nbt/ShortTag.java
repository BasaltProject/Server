// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.nbt;

import java.io.DataOutput;
import java.io.IOException;

public final class ShortTag implements NumericTag {
    public static final ShortTag ZERO = new ShortTag((short) 0);
    private final short value;

    private ShortTag(short value) {
        this.value = value;
    }

    public static ShortTag valueOf(short value) {
        return new ShortTag(value);
    }

    public short value() {
        return value;
    }

    @Override
    public byte getId() {
        return TAG_SHORT;
    }

    @Override
    public void write(DataOutput output) throws IOException {
        output.writeShort(value);
    }

    @Override
    public ShortTag copy() {
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
        return other instanceof ShortTag tag && tag.value == value;
    }

    @Override
    public int hashCode() {
        return Long.hashCode((long) value);
    }

    @Override
    public String toString() {
        return value + "s";
    }
    public static final TagType<ShortTag> TYPE = org.basaltmc.bukkit.facade.NbtSupport.type("SHORT", "TAG_Short",
        (in, acc) -> {
            acc.accountBytes(10);
            return ShortTag.valueOf(in.readShort());
        });
    public int sizeInBytes() {
        return 10;
    }
    public TagType<ShortTag> getType() {
        return TYPE;
    }
    public void accept(TagVisitor visitor) {
        visitor.visitShort(this);
    }
    public StreamTagVisitor.ValueResult accept(StreamTagVisitor visitor) {
        return visitor.visit(value);
    }
    static class Cache {
    }
}
