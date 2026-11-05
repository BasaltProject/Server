// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network;

import io.netty.buffer.ByteBuf;
import io.netty.channel.ChannelHandlerContext;
import io.netty.handler.codec.MessageToByteEncoder;
import java.util.zip.Deflater;

public class CompressionEncoder extends MessageToByteEncoder<ByteBuf> {
    private final byte[] scratch = new byte[8192];
    private final Deflater deflater;
    private int threshold;

    public CompressionEncoder(int threshold) {
        this.threshold = threshold;
        this.deflater = new Deflater();
    }

    protected void encode(ChannelHandlerContext ctx, ByteBuf uncompressed, ByteBuf out) {
        int length = uncompressed.readableBytes();
        if (length < threshold) {
            VarInt.write(out, 0);
            out.writeBytes(uncompressed);
            return;
        }
        byte[] input = new byte[length];
        uncompressed.readBytes(input);
        VarInt.write(out, length);
        deflater.setInput(input, 0, length);
        deflater.finish();
        while (!deflater.finished()) {
            out.writeBytes(scratch, 0, deflater.deflate(scratch));
        }
        deflater.reset();
    }

    public int getThreshold() {
        return threshold;
    }

    public void setThreshold(int threshold) {
        this.threshold = threshold;
    }
}
