// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.network;

import net.minecraft.network.Connection;
import net.minecraft.network.DisconnectionDetails;
import net.minecraft.network.chat.Component;
import net.minecraft.network.protocol.ping.ClientboundPongResponsePacket;
import net.minecraft.network.protocol.ping.ServerboundPingRequestPacket;
import net.minecraft.network.protocol.status.ClientboundStatusResponsePacket;
import net.minecraft.network.protocol.status.ServerStatus;
import net.minecraft.network.protocol.status.ServerStatusPacketListener;
import net.minecraft.network.protocol.status.ServerboundStatusRequestPacket;

public class ServerStatusPacketListenerImpl implements ServerStatusPacketListener {
    private static final Component DISCONNECT_REASON = Component.translatable("multiplayer.status.request_handled");
    private final ServerStatus status;
    private final Connection connection;
    private boolean answered;

    public ServerStatusPacketListenerImpl(ServerStatus status, Connection connection) {
        this.status = status;
        this.connection = connection;
    }

    public void onDisconnect(DisconnectionDetails details) {}

    public boolean isAcceptingMessages() {
        return connection.isConnected();
    }

    public void handleStatusRequest(ServerboundStatusRequestPacket packet) {
        if (answered) {
            connection.disconnect(DISCONNECT_REASON);
            return;
        }
        answered = true;
        connection.send(new ClientboundStatusResponsePacket(status));
    }

    public void handlePingRequest(ServerboundPingRequestPacket packet) {
        connection.send(new ClientboundPongResponsePacket(packet.getTime()));
        connection.disconnect(DISCONNECT_REASON);
    }
}
