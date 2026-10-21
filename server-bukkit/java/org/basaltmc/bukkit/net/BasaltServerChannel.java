package org.basaltmc.bukkit.net;

import io.netty.channel.AbstractServerChannel;
import io.netty.channel.ChannelConfig;
import io.netty.channel.DefaultChannelConfig;
import io.netty.channel.EventLoop;
import java.net.InetSocketAddress;
import java.net.SocketAddress;

final class BasaltServerChannel extends AbstractServerChannel {
    private final ChannelConfig config = new DefaultChannelConfig(this);
    private final InetSocketAddress local = new InetSocketAddress("0.0.0.0", 25565);
    private volatile boolean open = true;

    @Override
    protected boolean isCompatible(EventLoop loop) {
        return true;
    }

    @Override
    protected SocketAddress localAddress0() {
        return local;
    }

    @Override
    protected void doBind(SocketAddress localAddress) {}

    @Override
    protected void doClose() {
        open = false;
    }

    @Override
    protected void doBeginRead() {}

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
}
