// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network;

import io.netty.buffer.ByteBuf;

public class VarLong {
    private static final int kMaxBytes = 10;

    public static int getByteSize(long value) {
        return Math.max(1, (Long.SIZE - Long.numberOfLeadingZeros(value) + 6) / 7);
    }

    public static boolean hasContinuationBit(byte in) {
        return (in & 0x80) != 0;
    }

    public static long read(ByteBuf input) {
        long result = 0;
        for (int index = 0; index < kMaxBytes; index++) {
            byte part = input.readByte();
            result |= (long) (part & 0x7F) << (index * 7);
            if (!hasContinuationBit(part)) {
                return result;
            }
        }
        throw new RuntimeException("VarLong too big");
    }

    public static ByteBuf write(ByteBuf output, long value) {
        while ((value & ~0x7FL) != 0) {
            output.writeByte((int) (value & 0x7F) | 0x80);
            value >>>= 7;
        }
        return output.writeByte((int) value);
    }
}
