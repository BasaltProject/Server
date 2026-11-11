// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.network;

import net.minecraft.network.Connection;
import net.minecraft.network.protocol.Packet;
import org.basaltmc.bukkit.Unsupported;
import com.mojang.authlib.GameProfile;
import io.netty.channel.ChannelFutureListener;
import net.minecraft.ReportedException;
import net.minecraft.network.DisconnectionDetails;
import net.minecraft.network.chat.Component;
import net.minecraft.network.protocol.common.ServerCommonPacketListener;
import net.minecraft.network.protocol.common.ServerboundCustomClickActionPacket;
import net.minecraft.network.protocol.common.ServerboundCustomPayloadPacket;
import net.minecraft.network.protocol.common.ServerboundKeepAlivePacket;
import net.minecraft.network.protocol.common.ServerboundPongPacket;
import net.minecraft.network.protocol.common.ServerboundResourcePackPacket;
import net.minecraft.network.protocol.cookie.ServerboundCookieResponsePacket;
import net.minecraft.server.MinecraftServer;
import net.minecraft.network.protocol.common.ServerboundClientInformationPacket;
import net.minecraft.server.level.ClientInformation;

public class ServerCommonPacketListenerImpl implements ServerCommonPacketListener {
    protected final Connection connection;

    public ServerCommonPacketListenerImpl(Connection connection) {
        this.server = MinecraftServer.getServer();
        this.cookie = null;
        this.connection = connection;
    }
    public void send(Packet<?> packet) {
        connection.send(packet);
    }
    public static final int LATENCY_CHECK_INTERVAL = 15000;
    static final Component DISCONNECT_UNEXPECTED_QUERY = Component.translatable("multiplayer.disconnect.unexpected_query_response");
    protected final MinecraftServer server;
    protected final CommonListenerCookie cookie;
    public ServerCommonPacketListenerImpl(MinecraftServer server, Connection connection, CommonListenerCookie cookie) {
        this.server = server;
        this.connection = connection;
        this.cookie = cookie;
    }
    public void onDisconnect(DisconnectionDetails details) {
    }
    public void onPacketError(Packet packet, Exception e) throws ReportedException {
        throw new IllegalStateException("Packet error", e);
    }
    public void handleClientInformation(ServerboundClientInformationPacket packet) {
        forward(packet);
    }
    public void handleKeepAlive(ServerboundKeepAlivePacket packet) {
    }
    public void handlePong(ServerboundPongPacket serverboundPongPacket) {
    }
    public void handleCustomPayload(ServerboundCustomPayloadPacket packet) {
        forward(packet);
    }
    public void handleCustomClickAction(ServerboundCustomClickActionPacket packet) {
        forward(packet);
    }
    public void handleResourcePackResponse(ServerboundResourcePackPacket packet) {
        forward(packet);
    }
    public void handleCookieResponse(ServerboundCookieResponsePacket packet) {
        forward(packet);
    }
    protected void forward(Packet<?> packet) {
        org.basaltmc.bukkit.net.ChannelBridge.receiveFrom(connection.getChannel(), packet);
    }
    protected void keepConnectionAlive() {
    }
    public void suspendFlushing() {
    }
    public void resumeFlushing() {
    }
    public void send(Packet<?> packet, ChannelFutureListener listener) {
        connection.send(packet, listener);
    }
    public void disconnect(Component reason) {
        connection.disconnect(reason);
    }
    public void disconnect(DisconnectionDetails details) {
        connection.disconnect(details);
    }
    protected boolean isSingleplayerOwner() {
        return false;
    }
    protected GameProfile playerProfile() {
        return null;
    }
    public GameProfile getOwner() {
        return playerProfile();
    }
    public int latency() {
        return 0;
    }

    public MinecraftServer server() {
        return server;
    }
    protected CommonListenerCookie createCookie(ClientInformation clientInformation) {
        return new CommonListenerCookie(playerProfile(), latency(), clientInformation, cookie != null && cookie.transferred());
    }
}
