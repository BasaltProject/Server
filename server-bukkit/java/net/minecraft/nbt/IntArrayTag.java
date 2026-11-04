// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.nbt;

import java.io.DataOutput;
import java.io.IOException;
import java.util.Arrays;
import java.util.Optional;

public final class IntArrayTag implements CollectionTag {
    private int[] data;

    public IntArrayTag(int[] data) {
        this.data = data;
    }

    public int[] getAsIntArray() {
        return data;
    }

    @Override
    public byte getId() {
        return TAG_INT_ARRAY;
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
        for (int value : data) {
            output.writeInt(value);
        }
    }

    @Override
    public IntArrayTag copy() {
        return new IntArrayTag(data.clone());
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof IntArrayTag tag && Arrays.equals(tag.data, data);
    }

    @Override
    public int hashCode() {
        return Arrays.hashCode(data);
    }

    @Override
    public String toString() {
        return new StringTagVisitor().visit(this);
    }
    public static final TagType<IntArrayTag> TYPE = org.basaltmc.bukkit.facade.NbtSupport.type("INT[]", "TAG_Int_Array",
        (in, acc) -> {
            acc.accountBytes(24);
            int length = in.readInt();
            if (length < 0) {
                throw new java.io.IOException("negative array length " + length);
            }
            acc.accountBytes(4, length);
            int[] values = new int[length];
            for (int i = 0; i < length; i++) {
                values[i] = in.readInt();
            }
            return new IntArrayTag(values);
        });
    public int sizeInBytes() {
        return 24 + 4 * data.length;
    }
    public TagType<IntArrayTag> getType() {
        return TYPE;
    }
    public void accept(TagVisitor visitor) {
        visitor.visitIntArray(this);
    }
    public IntTag get(int index) {
        return IntTag.valueOf(data[index]);
    }
    public boolean setTag(int index, Tag tag) {
        if (tag instanceof NumericTag number) {
            data[index] = number.intValue();
            return true;
        }
        return false;
    }
    public boolean addTag(int index, Tag tag) {
        if (tag instanceof NumericTag number) {
            int[] grown = new int[data.length + 1];
            System.arraycopy(data, 0, grown, 0, index);
            grown[index] = number.intValue();
            System.arraycopy(data, index, grown, index + 1, data.length - index);
            data = grown;
            return true;
        }
        return false;
    }
    public IntTag remove(int index) {
        int removed = data[index];
        int[] shrunk = new int[data.length - 1];
        System.arraycopy(data, 0, shrunk, 0, index);
        System.arraycopy(data, index + 1, shrunk, index, data.length - index - 1);
        data = shrunk;
        return IntTag.valueOf(removed);
    }
    public void clear() {
        data = new int[0];
    }
    public Optional<int[]> asIntArray() {
        return Optional.of(data);
    }
    public StreamTagVisitor.ValueResult accept(StreamTagVisitor visitor) {
        return visitor.visit(data);
    }
}
