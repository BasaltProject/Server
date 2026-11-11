// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.network;

import net.minecraft.SharedConstants;
import net.minecraft.network.Connection;
import net.minecraft.network.DisconnectionDetails;
import net.minecraft.network.chat.Component;
import net.minecraft.network.protocol.handshake.ClientIntentionPacket;
import net.minecraft.network.protocol.handshake.ServerHandshakePacketListener;
import net.minecraft.network.protocol.login.ClientboundLoginDisconnectPacket;
import net.minecraft.network.protocol.login.LoginProtocols;
import net.minecraft.network.protocol.status.ServerStatus;
import net.minecraft.network.protocol.status.StatusProtocols;
import net.minecraft.server.MinecraftServer;

public class ServerHandshakePacketListenerImpl implements ServerHandshakePacketListener {
    private static final Component IGNORE_STATUS_REASON = Component.translatable("disconnect.ignoring_status_request");
    private static final int FIRST_MODERN_PROTOCOL = 754;
    private final MinecraftServer server;
    private final Connection connection;

    public ServerHandshakePacketListenerImpl(MinecraftServer server, Connection connection) {
        this.server = server;
        this.connection = connection;
    }

    public void handleIntention(ClientIntentionPacket packet) {
        switch (packet.intention()) {
            case LOGIN -> beginLogin(packet, false);
            case STATUS -> {
                ServerStatus status = server.getStatus();
                connection.setupOutboundProtocol(StatusProtocols.CLIENTBOUND);
                if (server.repliesToStatus() && status != null) {
                    connection.setupInboundProtocol(StatusProtocols.SERVERBOUND, new ServerStatusPacketListenerImpl(status, connection));
                } else {
                    connection.disconnect(IGNORE_STATUS_REASON);
                }
            }
            case TRANSFER -> {
                if (server.acceptsTransfers()) {
                    beginLogin(packet, true);
                } else {
                    connection.setupOutboundProtocol(LoginProtocols.CLIENTBOUND);
                    Component reason = Component.translatable("multiplayer.disconnect.transfers_disabled");
                    connection.send(new ClientboundLoginDisconnectPacket(reason));
                    connection.disconnect(reason);
                }
            }
            default -> throw new UnsupportedOperationException("Invalid intention " + packet.intention());
        }
    }

    private void beginLogin(ClientIntentionPacket packet, boolean transfer) {
        connection.setupOutboundProtocol(LoginProtocols.CLIENTBOUND);
        int current = SharedConstants.getCurrentVersion().protocolVersion();
        if (packet.protocolVersion() == current) {
            connection.setupInboundProtocol(LoginProtocols.SERVERBOUND, new ServerLoginPacketListenerImpl(server, connection, transfer));
            return;
        }
        String key = packet.protocolVersion() < FIRST_MODERN_PROTOCOL ? "multiplayer.disconnect.outdated_client" : "multiplayer.disconnect.incompatible";
        Component reason = Component.translatable(key, SharedConstants.getCurrentVersion().name());
        connection.send(new ClientboundLoginDisconnectPacket(reason));
        connection.disconnect(reason);
    }

    public void onDisconnect(DisconnectionDetails details) {}

    public boolean isAcceptingMessages() {
        return connection.isConnected();
    }
}
