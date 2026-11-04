// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network;
import io.netty.buffer.ByteBuf;
public class VarInt {
    public static final int MAX_VARINT_SIZE = 5;
    public static int getByteSize(int value) {
        for (int size = 1; size < MAX_VARINT_SIZE; size++) {
            if ((value & -1 << size * 7) == 0) {
                return size;
            }
        }
        return MAX_VARINT_SIZE;
    }
    public static boolean hasContinuationBit(byte in) {
        return (in & 128) == 128;
    }
    public static int read(ByteBuf input) {
        int value = 0;
        int shift = 0;
        byte part;
        do {
            part = input.readByte();
            value |= (part & 127) << shift++ * 7;
            if (shift > MAX_VARINT_SIZE) {
                throw new RuntimeException("VarInt too big");
            }
        } while (hasContinuationBit(part));
        return value;
    }
    public static ByteBuf write(ByteBuf output, int value) {
        while ((value & -128) != 0) {
            output.writeByte(value & 127 | 128);
            value >>>= 7;
        }
        output.writeByte(value);
        return output;
    }
}
