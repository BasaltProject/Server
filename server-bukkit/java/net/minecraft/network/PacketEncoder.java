// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network;

import io.netty.buffer.ByteBuf;
import io.netty.channel.ChannelHandlerContext;
import io.netty.handler.codec.MessageToByteEncoder;
import net.minecraft.network.protocol.Packet;

public class PacketEncoder<T extends PacketListener> extends MessageToByteEncoder<Packet<T>> {
    private final ProtocolInfo<T> protocolInfo;

    public PacketEncoder(ProtocolInfo<T> protocolInfo) {
        this.protocolInfo = protocolInfo;
    }

    protected void encode(ChannelHandlerContext ctx, Packet<T> packet, ByteBuf output) throws Exception {
        protocolInfo.codec().encode(output, packet);
    }
}
