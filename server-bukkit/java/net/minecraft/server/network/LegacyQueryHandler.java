// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.network;

import io.netty.buffer.ByteBuf;
import io.netty.channel.ChannelFutureListener;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.ChannelInboundHandlerAdapter;
import java.util.Locale;
import net.minecraft.server.ServerInfo;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class LegacyQueryHandler extends ChannelInboundHandlerAdapter {
    private static final Logger LOG = LoggerFactory.getLogger(LegacyQueryHandler.class);
    private static final String SEP = String.valueOf((char) 0xA7);
    private final ServerInfo server;

    public LegacyQueryHandler(ServerInfo server) {
        this.server = server;
    }

    public void channelRead(ChannelHandlerContext ctx, Object msg) {
        ByteBuf in = (ByteBuf) msg;
        in.markReaderIndex();
        boolean passThrough = true;
        try {
            if (in.readUnsignedByte() != LegacyProtocolUtils.GET_INFO_PACKET_ID) {
                return;
            }
            String body;
            if (in.readableBytes() == 0) {
                LOG.debug("Ping: (<1.3.x) from {}", ctx.channel().remoteAddress());
                body = server.getMotd() + SEP + server.getPlayerCount() + SEP + server.getMaxPlayers();
            } else {
                if (in.readUnsignedByte() != LegacyProtocolUtils.GET_INFO_PACKET_VERSION_1) {
                    return;
                }
                if (in.isReadable()) {
                    if (!readPingHost(in)) {
                        return;
                    }
                    LOG.debug("Ping: (1.6) from {}", ctx.channel().remoteAddress());
                } else {
                    LOG.debug("Ping: (1.4-1.5.x) from {}", ctx.channel().remoteAddress());
                }
                body = String.format(Locale.ROOT, SEP + "1\u0000%d\u0000%s\u0000%s\u0000%d\u0000%d", LegacyProtocolUtils.FAKE_PROTOCOL_VERSION,
                    server.getServerVersion(), server.getMotd(), server.getPlayerCount(), server.getMaxPlayers());
            }
            ByteBuf out = ctx.alloc().buffer();
            out.writeByte(LegacyProtocolUtils.DISCONNECT_PACKET_ID);
            LegacyProtocolUtils.writeLegacyString(out, body);
            ctx.pipeline().firstContext().writeAndFlush(out).addListener(ChannelFutureListener.CLOSE);
            in.release();
            passThrough = false;
        } catch (RuntimeException unreadable) {
            return;
        } finally {
            if (passThrough) {
                in.resetReaderIndex();
                ctx.channel().pipeline().remove(this);
                ctx.fireChannelRead(msg);
            }
        }
    }

    private static boolean readPingHost(ByteBuf in) {
        if (in.readUnsignedByte() != 250 || !"MC|PingHost".equals(LegacyProtocolUtils.readLegacyString(in))) {
            return false;
        }
        int payload = in.readUnsignedShort();
        if (in.readableBytes() != payload || in.readUnsignedByte() < 73) {
            return false;
        }
        LegacyProtocolUtils.readLegacyString(in);
        return in.readInt() <= 65535;
    }
}
