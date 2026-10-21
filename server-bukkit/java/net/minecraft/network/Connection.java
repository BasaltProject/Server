package net.minecraft.network;

import io.netty.channel.Channel;

/** The per-client connection object whose single channel field plugins read to reach the netty pipeline. */
public class Connection {
    private Channel channel;

    public Connection(Channel channel) {
        this.channel = channel;
    }

    public Channel getChannel() {
        return channel;
    }
}
