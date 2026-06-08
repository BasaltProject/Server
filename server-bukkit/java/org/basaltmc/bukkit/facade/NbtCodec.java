package org.basaltmc.bukkit.facade;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInput;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import net.minecraft.nbt.ByteArrayTag;
import net.minecraft.nbt.ByteTag;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.DoubleTag;
import net.minecraft.nbt.EndTag;
import net.minecraft.nbt.FloatTag;
import net.minecraft.nbt.IntArrayTag;
import net.minecraft.nbt.IntTag;
import net.minecraft.nbt.ListTag;
import net.minecraft.nbt.LongArrayTag;
import net.minecraft.nbt.LongTag;
import net.minecraft.nbt.ShortTag;
import net.minecraft.nbt.StringTag;
import net.minecraft.nbt.Tag;

public final class NbtCodec {
    private NbtCodec() {}

    public static byte[] toBytes(CompoundTag tag) {
        try {
            ByteArrayOutputStream bytes = new ByteArrayOutputStream();
            DataOutputStream out = new DataOutputStream(bytes);
            out.writeByte(Tag.TAG_COMPOUND);
            out.writeUTF("");
            tag.write(out);
            return bytes.toByteArray();
        } catch (IOException e) {
            throw new IllegalStateException(e);
        }
    }

    public static CompoundTag fromBytes(byte[] bytes) {
        if (bytes == null || bytes.length == 0) {
            return null;
        }
        try {
            DataInputStream in = new DataInputStream(new ByteArrayInputStream(bytes));
            byte type = in.readByte();
            if (type != Tag.TAG_COMPOUND) {
                return null;
            }
            in.readUTF();
            return (CompoundTag) read(in, type);
        } catch (IOException e) {
            throw new IllegalStateException(e);
        }
    }

    public static Tag read(DataInput in, byte type) throws IOException {
        switch (type) {
            case Tag.TAG_END:
                return EndTag.INSTANCE;
            case Tag.TAG_BYTE:
                return ByteTag.valueOf(in.readByte());
            case Tag.TAG_SHORT:
                return ShortTag.valueOf(in.readShort());
            case Tag.TAG_INT:
                return IntTag.valueOf(in.readInt());
            case Tag.TAG_LONG:
                return LongTag.valueOf(in.readLong());
            case Tag.TAG_FLOAT:
                return FloatTag.valueOf(in.readFloat());
            case Tag.TAG_DOUBLE:
                return DoubleTag.valueOf(in.readDouble());
            case Tag.TAG_BYTE_ARRAY: {
                byte[] data = new byte[in.readInt()];
                in.readFully(data);
                return new ByteArrayTag(data);
            }
            case Tag.TAG_STRING:
                return StringTag.valueOf(in.readUTF());
            case Tag.TAG_LIST: {
                byte elements = in.readByte();
                int length = in.readInt();
                ListTag list = new ListTag();
                for (int i = 0; i < length; i++) {
                    list.add(read(in, elements));
                }
                return list;
            }
            case Tag.TAG_COMPOUND: {
                CompoundTag compound = new CompoundTag();
                for (byte next = in.readByte(); next != Tag.TAG_END; next = in.readByte()) {
                    String name = in.readUTF();
                    compound.put(name, read(in, next));
                }
                return compound;
            }
            case Tag.TAG_INT_ARRAY: {
                int[] data = new int[in.readInt()];
                for (int i = 0; i < data.length; i++) {
                    data[i] = in.readInt();
                }
                return new IntArrayTag(data);
            }
            case Tag.TAG_LONG_ARRAY: {
                long[] data = new long[in.readInt()];
                for (int i = 0; i < data.length; i++) {
                    data[i] = in.readLong();
                }
                return new LongArrayTag(data);
            }
            default:
                throw new IOException("Unknown NBT tag type " + type);
        }
    }
}
