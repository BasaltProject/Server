// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.network;

import io.netty.channel.Channel;
import io.netty.channel.DefaultEventLoopGroup;
import io.netty.channel.EventLoopGroup;
import io.netty.channel.IoHandlerFactory;
import io.netty.channel.ServerChannel;
import io.netty.channel.local.LocalChannel;
import io.netty.channel.local.LocalServerChannel;
import io.netty.channel.nio.NioEventLoopGroup;
import io.netty.channel.socket.nio.NioServerSocketChannel;
import io.netty.channel.socket.nio.NioSocketChannel;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.Function;

/** Only the NIO and local transports exist in the facade; the native transports belong to the C++ network layer. */
public abstract class EventLoopGroupHolder {
    private static final EventLoopGroupHolder NIO = new EventLoopGroupHolder("NIO", NioSocketChannel.class, NioServerSocketChannel.class,
        factory -> new NioEventLoopGroup(0, factory)) {
        protected IoHandlerFactory ioHandlerFactory() {
            return new IoHandlerFactory() {};
        }
    };
    private static final EventLoopGroupHolder LOCAL = new EventLoopGroupHolder("Local", LocalChannel.class, LocalServerChannel.class,
        factory -> new DefaultEventLoopGroup(0, factory)) {
        protected IoHandlerFactory ioHandlerFactory() {
            return new IoHandlerFactory() {};
        }
    };
    private final String type;
    private final Class<? extends Channel> channelCls;
    private final Class<? extends ServerChannel> serverChannelCls;
    private final Function<ThreadFactory, EventLoopGroup> maker;
    private volatile EventLoopGroup group;

    public static EventLoopGroupHolder remote(boolean allowNativeTransport) {
        return NIO;
    }

    public static EventLoopGroupHolder local() {
        return LOCAL;
    }

    private EventLoopGroupHolder(String type, Class<? extends Channel> channelCls, Class<? extends ServerChannel> serverChannelCls,
        Function<ThreadFactory, EventLoopGroup> maker) {
        this.type = type;
        this.channelCls = channelCls;
        this.serverChannelCls = serverChannelCls;
        this.maker = maker;
    }

    protected abstract IoHandlerFactory ioHandlerFactory();

    public EventLoopGroup eventLoopGroup() {
        EventLoopGroup current = group;
        if (current == null) {
            synchronized (this) {
                current = group;
                if (current == null) {
                    AtomicInteger index = new AtomicInteger(1);
                    current = maker.apply(task -> {
                        Thread thread = new Thread(task, "Netty " + type + " IO #" + index.getAndIncrement());
                        thread.setDaemon(true);
                        return thread;
                    });
                    group = current;
                }
            }
        }
        return current;
    }

    public Class<? extends Channel> channelCls() {
        return channelCls;
    }

    public Class<? extends ServerChannel> serverChannelCls() {
        return serverChannelCls;
    }
}
