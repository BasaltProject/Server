// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.nbt;

import java.io.DataOutput;
import java.io.IOException;
import java.util.Arrays;
import java.util.Optional;

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
        return new StringTagVisitor().visit(this);
    }
    public static final TagType<LongArrayTag> TYPE = org.basaltmc.bukkit.facade.NbtSupport.type("LONG[]", "TAG_Long_Array",
        (in, acc) -> {
            acc.accountBytes(24);
            int length = in.readInt();
            if (length < 0) {
                throw new java.io.IOException("negative array length " + length);
            }
            acc.accountBytes(8, length);
            long[] values = new long[length];
            for (int i = 0; i < length; i++) {
                values[i] = in.readLong();
            }
            return new LongArrayTag(values);
        });
    public int sizeInBytes() {
        return 24 + 8 * data.length;
    }
    public TagType<LongArrayTag> getType() {
        return TYPE;
    }
    public void accept(TagVisitor visitor) {
        visitor.visitLongArray(this);
    }
    public LongTag get(int index) {
        return LongTag.valueOf(data[index]);
    }
    public boolean setTag(int index, Tag tag) {
        if (tag instanceof NumericTag number) {
            data[index] = number.longValue();
            return true;
        }
        return false;
    }
    public boolean addTag(int index, Tag tag) {
        if (tag instanceof NumericTag number) {
            long[] grown = new long[data.length + 1];
            System.arraycopy(data, 0, grown, 0, index);
            grown[index] = number.longValue();
            System.arraycopy(data, index, grown, index + 1, data.length - index);
            data = grown;
            return true;
        }
        return false;
    }
    public LongTag remove(int index) {
        long removed = data[index];
        long[] shrunk = new long[data.length - 1];
        System.arraycopy(data, 0, shrunk, 0, index);
        System.arraycopy(data, index + 1, shrunk, index, data.length - index - 1);
        data = shrunk;
        return LongTag.valueOf(removed);
    }
    public void clear() {
        data = new long[0];
    }
    public Optional<long[]> asLongArray() {
        return Optional.of(data);
    }
    public StreamTagVisitor.ValueResult accept(StreamTagVisitor visitor) {
        return visitor.visit(data);
    }
}
