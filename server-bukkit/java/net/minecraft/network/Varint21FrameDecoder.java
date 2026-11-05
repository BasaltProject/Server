// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network;

import io.netty.buffer.ByteBuf;
import io.netty.channel.ChannelHandlerContext;
import io.netty.handler.codec.ByteToMessageDecoder;
import io.netty.handler.codec.CorruptedFrameException;
import java.util.List;

public class Varint21FrameDecoder extends ByteToMessageDecoder {
    private final BandwidthDebugMonitor monitor;

    public Varint21FrameDecoder(BandwidthDebugMonitor monitor) {
        this.monitor = monitor;
    }

    protected void handlerRemoved0(ChannelHandlerContext ctx) {
    }

    protected void decode(ChannelHandlerContext ctx, ByteBuf in, List<Object> out) {
        in.markReaderIndex();
        int length = 0;
        for (int shift = 0; shift < 21; shift += 7) {
            if (!in.isReadable()) {
                in.resetReaderIndex();
                return;
            }
            byte part = in.readByte();
            length |= (part & 0x7F) << shift;
            if (part >= 0) {
                if (length == 0) {
                    throw new CorruptedFrameException("Empty packet");
                }
                if (in.readableBytes() < length) {
                    in.resetReaderIndex();
                    return;
                }
                if (monitor != null) {
                    monitor.onReceive(length + shift / 7 + 1);
                }
                out.add(in.readRetainedSlice(length));
                return;
            }
        }
        throw new CorruptedFrameException("length wider than 21-bit");
    }
}
