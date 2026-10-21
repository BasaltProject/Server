package net.minecraft.server.network;

import io.netty.channel.ChannelFuture;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.network.Connection;
import org.basaltmc.bukkit.net.ChannelBridge;

public class ServerConnectionListener {
    private List<ChannelFuture> channels = new ChannelBridge.ServerChannels();
    private List<Connection> connections = new ArrayList<>();

    public ServerConnectionListener() {
        ChannelBridge.attach(channels, connections);
    }
}
