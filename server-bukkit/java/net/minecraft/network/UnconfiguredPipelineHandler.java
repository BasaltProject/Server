// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network;

import io.netty.buffer.ByteBuf;
import io.netty.channel.ChannelDuplexHandler;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.ChannelOutboundHandlerAdapter;
import io.netty.channel.ChannelPromise;
import io.netty.handler.codec.DecoderException;
import io.netty.handler.codec.EncoderException;
import io.netty.util.ReferenceCountUtil;
import net.minecraft.network.protocol.Packet;

public class UnconfiguredPipelineHandler {
    public static <T extends PacketListener> InboundConfigurationTask setupInboundProtocol(ProtocolInfo<T> protocolInfo) {
        return ctx -> {
            ctx.pipeline().replace(ctx.name(), "decoder", new PacketDecoder<>(protocolInfo));
            ctx.channel().config().setAutoRead(true);
        };
    }

    public static <T extends PacketListener> OutboundConfigurationTask setupOutboundProtocol(ProtocolInfo<T> codecData) {
        return ctx -> ctx.pipeline().replace(ctx.name(), "encoder", new PacketEncoder<>(codecData));
    }

    public static class Inbound extends ChannelDuplexHandler {
        public void channelRead(ChannelHandlerContext ctx, Object msg) {
            if (msg instanceof ByteBuf || msg instanceof Packet) {
                ReferenceCountUtil.release(msg);
                throw new DecoderException("No inbound protocol is configured, cannot process " + msg);
            }
            ctx.fireChannelRead(msg);
        }

        public void write(ChannelHandlerContext ctx, Object msg, ChannelPromise promise) throws Exception {
            if (!(msg instanceof InboundConfigurationTask task)) {
                ctx.write(msg, promise);
                return;
            }
            try {
                task.run(ctx);
            } finally {
                ReferenceCountUtil.release(msg);
            }
            promise.setSuccess();
        }
    }

    public static class Outbound extends ChannelOutboundHandlerAdapter {
        public void write(ChannelHandlerContext ctx, Object msg, ChannelPromise promise) throws Exception {
            if (msg instanceof Packet) {
                ReferenceCountUtil.release(msg);
                throw new EncoderException("No outbound protocol is configured, cannot process " + msg);
            }
            if (!(msg instanceof OutboundConfigurationTask task)) {
                ctx.write(msg, promise);
                return;
            }
            try {
                task.run(ctx);
            } finally {
                ReferenceCountUtil.release(msg);
            }
            promise.setSuccess();
        }
    }

    @FunctionalInterface
    public interface InboundConfigurationTask {
        void run(ChannelHandlerContext ctx);

        default InboundConfigurationTask andThen(InboundConfigurationTask otherTask) {
            return ctx -> {
                run(ctx);
                otherTask.run(ctx);
            };
        }
    }

    @FunctionalInterface
    public interface OutboundConfigurationTask {
        void run(ChannelHandlerContext ctx);

        default OutboundConfigurationTask andThen(OutboundConfigurationTask otherTask) {
            return ctx -> {
                run(ctx);
                otherTask.run(ctx);
            };
        }
    }
}
