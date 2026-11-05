// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network;

import io.netty.channel.ChannelFutureListener;
import java.util.function.Supplier;
import net.minecraft.network.protocol.Packet;

public class PacketSendListener {
    public static ChannelFutureListener thenRun(Runnable runnable) {
        return future -> {
            runnable.run();
            if (!future.isSuccess()) {
                future.channel().pipeline().fireExceptionCaught(future.cause());
            }
        };
    }

    public static ChannelFutureListener exceptionallySend(Supplier<Packet<?>> handler) {
        return future -> {
            if (future.isSuccess()) {
                return;
            }
            Packet<?> fallback = handler.get();
            if (fallback != null) {
                future.channel().writeAndFlush(fallback).addListener(ChannelFutureListener.FIRE_EXCEPTION_ON_FAILURE);
            }
            future.channel().pipeline().fireExceptionCaught(future.cause());
        };
    }
}
