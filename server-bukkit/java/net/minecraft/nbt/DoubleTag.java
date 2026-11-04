// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
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
    public static final TagType<DoubleTag> TYPE = org.basaltmc.bukkit.facade.NbtSupport.type("DOUBLE", "TAG_Double",
        (in, acc) -> {
            acc.accountBytes(16);
            return DoubleTag.valueOf(in.readDouble());
        });
    public int sizeInBytes() {
        return 16;
    }
    public TagType<DoubleTag> getType() {
        return TYPE;
    }
    public void accept(TagVisitor visitor) {
        visitor.visitDouble(this);
    }
    public StreamTagVisitor.ValueResult accept(StreamTagVisitor visitor) {
        return visitor.visit(value);
    }
}
