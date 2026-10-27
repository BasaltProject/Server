package org.basaltmc.bukkit.net;
import io.netty.channel.ChannelHandlerContext;
import io.netty.handler.codec.MessageToMessageEncoder;
import net.minecraft.network.protocol.Packet;
public final class WirePacketEncoder extends MessageToMessageEncoder<Packet<?>> {
    protected void encode(ChannelHandlerContext context, Packet<?> packet, java.util.List<Object> out) {
        if (packet instanceof WirePacket wire)
            out.add(PacketCodec.frame(wire));
        else if (packet instanceof net.minecraft.network.protocol.BundlePacket<?> bundle) {
            java.util.List<io.netty.buffer.ByteBuf> frames = new java.util.ArrayList<>();
            try {
                frames.add(io.netty.buffer.Unpooled.buffer(1).writeByte(0));
                for (Packet<?> item : bundle.subPackets()) {
                    if (!(item instanceof WirePacket wire))
                        throw new IllegalArgumentException("Unsupported bundled packet: " + item.getClass());
                    frames.add(PacketCodec.frame(wire));
                }
                frames.add(io.netty.buffer.Unpooled.buffer(1).writeByte(0));
            } catch (RuntimeException | Error failure) {
                for (var frame : frames) frame.release();
                throw failure;
            }
            out.addAll(frames);
        } else
            throw new IllegalArgumentException("Unsupported packet: " + packet.getClass());
    }
}
