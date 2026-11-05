// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network;

import io.netty.buffer.ByteBuf;
import net.minecraft.world.phys.Vec3;
import org.basaltmc.bukkit.Natives;

public class LpVec3 {
    public static final double ABS_MAX_VALUE = 17179869183.0;
    public static final double ABS_MIN_VALUE = 3.051944088384301E-5;
    private static final int kLongestEncoding = 11;

    public static boolean hasContinuationBit(int in) {
        return (in & 4) == 4;
    }

    public static Vec3 read(ByteBuf input) {
        byte[] window = new byte[Math.min(input.readableBytes(), kLongestEncoding)];
        input.getBytes(input.readerIndex(), window);
        double[] decoded = Natives.lpVec3Decode(window, 0);
        if (decoded == null) {
            throw new io.netty.handler.codec.DecoderException("Truncated low precision vector");
        }
        input.skipBytes((int) decoded[3]);
        return new Vec3(decoded[0], decoded[1], decoded[2]);
    }

    public static void write(ByteBuf output, Vec3 value) {
        output.writeBytes(Natives.lpVec3Encode(value.x, value.y, value.z));
    }
}
