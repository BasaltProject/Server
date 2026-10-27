package net.minecraft.server.network;

import net.minecraft.network.Connection;
import net.minecraft.network.protocol.Packet;
import org.basaltmc.bukkit.Unsupported;

public class ServerCommonPacketListenerImpl {
    protected final Connection connection;

    public ServerCommonPacketListenerImpl(Connection connection) {
        this.connection = connection;
    }
    public void send(Packet<?> packet) {
        if (!(packet instanceof org.basaltmc.bukkit.net.WirePacket) && !(packet instanceof net.minecraft.network.protocol.BundlePacket<?>)) {
            throw Unsupported.hit("net.minecraft.server.network.ServerCommonPacketListenerImpl.send: " + packet.getClass().getName());
        }
        connection.getChannel().writeAndFlush(packet);
    }
}
