// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network;
import io.netty.channel.ChannelHandlerContext;
import io.netty.handler.codec.DecoderException;
import io.netty.handler.codec.MessageToMessageDecoder;
import java.util.List;
import net.minecraft.network.protocol.BundlerInfo;
import net.minecraft.network.protocol.Packet;
public class PacketBundlePacker extends MessageToMessageDecoder<Packet<?>> {
    private final BundlerInfo info;
    private BundlerInfo.Bundler open;
    public PacketBundlePacker(BundlerInfo bundlerInfo) {
        this.info = bundlerInfo;
    }
    protected void decode(ChannelHandlerContext ctx, Packet<?> msg, List<Object> out) throws Exception {
        if (open == null) {
            open = info.startPacketBundling(msg);
            if (open == null) {
                out.add(msg);
            }
            return;
        }
        Packet<?> done = open.addPacket(msg);
        if (done != null) {
            open = null;
            out.add(done);
        } else if (msg == null) {
            throw new DecoderException("Received a null packet inside a bundle");
        }
    }
}
