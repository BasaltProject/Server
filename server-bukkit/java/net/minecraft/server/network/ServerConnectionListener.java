// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.network;

import io.netty.channel.ChannelFuture;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.network.Connection;
import org.basaltmc.bukkit.net.ChannelBridge;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.ChannelInboundHandlerAdapter;
import java.io.IOException;
import java.net.InetAddress;
import java.net.SocketAddress;
import java.util.UUID;
import net.minecraft.server.MinecraftServer;

public class ServerConnectionListener {
    private List<ChannelFuture> channels = new ChannelBridge.ServerChannels();
    private List<Connection> connections = new ArrayList<>();
    private final UUID sessionId = UUID.randomUUID();

    public ServerConnectionListener() {
        ChannelBridge.attach(channels, connections);
    }
    public boolean running;
    public ServerConnectionListener(MinecraftServer server) {
    }
    public void startTcpServerListener(InetAddress address, int port) throws IOException {
    }
    public SocketAddress startMemoryChannel() {
        return new java.net.InetSocketAddress(java.net.InetAddress.getLoopbackAddress(), org.bukkit.Bukkit.getPort());
    }
    public void stop() {
        running = false;
    }
    public void stopTcpServerListener() {
        running = false;
    }
    public void tick() {
    }
    public MinecraftServer getServer() {
        return MinecraftServer.getServer();
    }
    public List<Connection> getConnections() {
        return connections;
    }
    public UUID getSessionId() {
        return sessionId;
    }
    static class LatencySimulator extends ChannelInboundHandlerAdapter {
        public LatencySimulator(int delay, int jitter) {
        }
        public void channelRead(ChannelHandlerContext ctx, Object msg) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.network.ServerConnectionListener.LatencySimulator#channelRead");
        }
        static record DelayedMessage(ChannelHandlerContext ctx, Object msg) {
        }
    }
}
