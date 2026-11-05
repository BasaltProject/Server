// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network;

import io.netty.buffer.ByteBuf;
import io.netty.channel.ChannelHandlerContext;
import io.netty.handler.codec.ByteToMessageDecoder;
import io.netty.handler.codec.DecoderException;
import java.util.List;
import net.minecraft.network.protocol.Packet;

public class PacketDecoder<T extends PacketListener> extends ByteToMessageDecoder implements ProtocolSwapHandler {
    private final ProtocolInfo<T> protocolInfo;

    public PacketDecoder(ProtocolInfo<T> protocolInfo) {
        this.protocolInfo = protocolInfo;
    }

    protected void decode(ChannelHandlerContext ctx, ByteBuf input, List<Object> out) throws Exception {
        int start = input.readableBytes();
        if (start == 0) {
            return;
        }
        Packet<? super T> packet = protocolInfo.codec().decode(input);
        if (input.isReadable()) {
            throw new DecoderException(packet.type() + " was larger than expected, found " + input.readableBytes() + " bytes extra");
        }
        out.add(packet);
    }
}
