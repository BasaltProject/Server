package org.basaltmc.bukkit.net;

import io.netty.channel.AbstractChannel;
import io.netty.channel.ChannelConfig;
import io.netty.channel.ChannelMetadata;
import io.netty.channel.ChannelOutboundBuffer;
import io.netty.channel.ChannelPromise;
import io.netty.channel.DefaultChannelConfig;
import io.netty.channel.EventLoop;
import java.net.InetSocketAddress;
import java.net.SocketAddress;

final class BasaltChannel extends AbstractChannel {
    private static final ChannelMetadata kMetadata = new ChannelMetadata(false);

    private final ChannelConfig config = new DefaultChannelConfig(this);
    private final InetSocketAddress remote;
    private final InetSocketAddress local;
    private final long connection;
    private volatile boolean open = true;
    private volatile boolean fromNative;

    BasaltChannel(long connection, InetSocketAddress remote, InetSocketAddress local) {
        super(null);
        this.connection = connection;
        this.remote = remote;
        this.local = local;
    }

    long connection() {
        return connection;
    }

    @Override
    protected AbstractUnsafe newUnsafe() {
        return new AbstractUnsafe() {
            @Override
            public void connect(SocketAddress remoteAddress, SocketAddress localAddress, ChannelPromise promise) {
                promise.setFailure(new UnsupportedOperationException());
            }
        };
    }

    @Override
    protected boolean isCompatible(EventLoop loop) {
        return true;
    }

    @Override
    protected SocketAddress localAddress0() {
        return local;
    }

    @Override
    protected SocketAddress remoteAddress0() {
        return remote;
    }

    @Override
    protected void doBind(SocketAddress localAddress) {}

    @Override
    protected void doDisconnect() {
        doClose();
    }

    @Override
    protected void doClose() {
        open = false;
        if (!fromNative) {
            ChannelBridge.closedByPlugin(connection);
        }
    }

    @Override
    protected void doBeginRead() {}

    @Override
    protected void doWrite(ChannelOutboundBuffer in) {
        while (in.remove()) {
            // Everything reaches the wire through the pipeline's last handler; nothing is left for the channel itself.
        }
    }

    void markClosedFromNative() {
        fromNative = true;
    }

    @Override
    public ChannelConfig config() {
        return config;
    }

    @Override
    public boolean isOpen() {
        return open;
    }

    @Override
    public boolean isActive() {
        return open;
    }

    @Override
    public ChannelMetadata metadata() {
        return kMetadata;
    }
}
