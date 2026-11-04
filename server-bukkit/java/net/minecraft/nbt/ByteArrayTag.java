// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.nbt;

import java.io.DataOutput;
import java.io.IOException;
import java.util.Arrays;
import java.util.Optional;

public final class ByteArrayTag implements CollectionTag {
    private byte[] data;

    public ByteArrayTag(byte[] data) {
        this.data = data;
    }

    public byte[] getAsByteArray() {
        return data;
    }

    @Override
    public byte getId() {
        return TAG_BYTE_ARRAY;
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
        for (byte value : data) {
            output.writeByte(value);
        }
    }

    @Override
    public ByteArrayTag copy() {
        return new ByteArrayTag(data.clone());
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof ByteArrayTag tag && Arrays.equals(tag.data, data);
    }

    @Override
    public int hashCode() {
        return Arrays.hashCode(data);
    }

    @Override
    public String toString() {
        return new StringTagVisitor().visit(this);
    }
    public static final TagType<ByteArrayTag> TYPE = org.basaltmc.bukkit.facade.NbtSupport.type("BYTE[]", "TAG_Byte_Array",
        (in, acc) -> {
            acc.accountBytes(24);
            int length = in.readInt();
            if (length < 0) {
                throw new java.io.IOException("negative array length " + length);
            }
            acc.accountBytes(1, length);
            byte[] values = new byte[length];
            for (int i = 0; i < length; i++) {
                values[i] = in.readByte();
            }
            return new ByteArrayTag(values);
        });
    public int sizeInBytes() {
        return 24 + 1 * data.length;
    }
    public TagType<ByteArrayTag> getType() {
        return TYPE;
    }
    public void accept(TagVisitor visitor) {
        visitor.visitByteArray(this);
    }
    public ByteTag get(int index) {
        return ByteTag.valueOf(data[index]);
    }
    public boolean setTag(int index, Tag tag) {
        if (tag instanceof NumericTag number) {
            data[index] = number.byteValue();
            return true;
        }
        return false;
    }
    public boolean addTag(int index, Tag tag) {
        if (tag instanceof NumericTag number) {
            byte[] grown = new byte[data.length + 1];
            System.arraycopy(data, 0, grown, 0, index);
            grown[index] = number.byteValue();
            System.arraycopy(data, index, grown, index + 1, data.length - index);
            data = grown;
            return true;
        }
        return false;
    }
    public ByteTag remove(int index) {
        byte removed = data[index];
        byte[] shrunk = new byte[data.length - 1];
        System.arraycopy(data, 0, shrunk, 0, index);
        System.arraycopy(data, index + 1, shrunk, index, data.length - index - 1);
        data = shrunk;
        return ByteTag.valueOf(removed);
    }
    public void clear() {
        data = new byte[0];
    }
    public Optional<byte[]> asByteArray() {
        return Optional.of(data);
    }
    public StreamTagVisitor.ValueResult accept(StreamTagVisitor visitor) {
        return visitor.visit(data);
    }
}
