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
import net.minecraft.server.level.ClientInformation;

public class ServerCommonPacketListenerImpl implements ServerCommonPacketListener {
    protected final Connection connection;

    public ServerCommonPacketListenerImpl(Connection connection) {
        this.connection = connection;
    }
    public void send(Packet<?> packet) {
        connection.send(packet);
    }
    public static final int LATENCY_CHECK_INTERVAL = 15000;
    static final Component DISCONNECT_UNEXPECTED_QUERY = null;
    protected final MinecraftServer server = null;
    public ServerCommonPacketListenerImpl(MinecraftServer server, Connection connection, CommonListenerCookie cookie) {
        this.connection = connection;
    }
    public void onDisconnect(DisconnectionDetails details) {
    }
    public void onPacketError(Packet packet, Exception e) throws ReportedException {
        throw new IllegalStateException("Packet error", e);
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
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.network.ServerCommonPacketListenerImpl#forward");
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
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.network.ServerCommonPacketListenerImpl#playerProfile");
    }
    public GameProfile getOwner() {
        return playerProfile();
    }
    public int latency() {
        return 0;
    }
    protected CommonListenerCookie createCookie(ClientInformation clientInformation) {
        return null;
    }
}
