// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network;

import io.netty.buffer.ByteBuf;
import io.netty.channel.ChannelHandler;
import io.netty.channel.ChannelHandlerContext;
import io.netty.handler.codec.EncoderException;
import io.netty.handler.codec.MessageToByteEncoder;

@ChannelHandler.Sharable
public class Varint21LengthFieldPrepender extends MessageToByteEncoder<ByteBuf> {
    public static final int MAX_VARINT21_BYTES = 3;

    protected void encode(ChannelHandlerContext ctx, ByteBuf msg, ByteBuf out) {
        int length = msg.readableBytes();
        int prefix = VarInt.getByteSize(length);
        if (prefix > MAX_VARINT21_BYTES) {
            throw new EncoderException("Packet too large: size " + length + " is over 8");
        }
        out.ensureWritable(prefix + length);
        VarInt.write(out, length);
        out.writeBytes(msg, msg.readerIndex(), length);
    }
}
