// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network;
import io.netty.buffer.ByteBuf;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.ChannelInboundHandlerAdapter;
public class MonitoredLocalFrameDecoder extends ChannelInboundHandlerAdapter {
    private final BandwidthDebugMonitor monitor;
    public MonitoredLocalFrameDecoder(BandwidthDebugMonitor monitor) {
        this.monitor = monitor;
    }
    public void channelRead(ChannelHandlerContext ctx, Object msg) {
        if (msg instanceof ByteBuf buffer) {
            monitor.onReceive(buffer.readableBytes());
        }
        ctx.fireChannelRead(msg);
    }
}
