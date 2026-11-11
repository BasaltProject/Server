// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network;
import io.netty.channel.ChannelHandlerContext;
import io.netty.handler.codec.MessageToMessageEncoder;
import java.util.List;
import net.minecraft.network.protocol.BundlerInfo;
import net.minecraft.network.protocol.Packet;
public class PacketBundleUnpacker extends MessageToMessageEncoder<Packet<?>> {
    private final BundlerInfo info;
    public PacketBundleUnpacker(BundlerInfo bundlerInfo) {
        this.info = bundlerInfo;
    }
    protected void encode(ChannelHandlerContext ctx, Packet<?> msg, List<Object> out) throws Exception {
        info.unbundlePacket(msg, out::add);
    }
}
