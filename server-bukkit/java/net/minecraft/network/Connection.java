// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network;

import io.netty.channel.Channel;
import io.netty.channel.ChannelFuture;
import io.netty.channel.ChannelFutureListener;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.ChannelPipeline;
import io.netty.channel.SimpleChannelInboundHandler;
import java.net.InetSocketAddress;
import java.net.SocketAddress;
import java.util.function.Consumer;
import javax.crypto.Cipher;
import net.minecraft.SharedConstants;
import net.minecraft.network.chat.Component;
import net.minecraft.network.protocol.handshake.ClientIntent;
import net.minecraft.network.protocol.handshake.ClientIntentionPacket;
import net.minecraft.network.protocol.login.LoginProtocols;
import net.minecraft.network.protocol.status.StatusProtocols;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketFlow;
import net.minecraft.network.protocol.login.ClientLoginPacketListener;
import net.minecraft.network.protocol.status.ClientStatusPacketListener;
import net.minecraft.server.network.EventLoopGroupHolder;
import net.minecraft.util.debugchart.LocalSampleLogger;
import org.slf4j.Marker;

/** The per-client connection object whose single channel field plugins read to reach the netty pipeline. */
public class Connection extends SimpleChannelInboundHandler<Packet<?>> {
    private Channel channel;
    private PacketFlow receiving = PacketFlow.SERVERBOUND;
    private PacketListener packetListener;
    private DisconnectionDetails disconnectionDetails;
    private boolean handledDisconnection;
    private long windowStart = System.nanoTime();
    private int sentInWindow;
    private int receivedInWindow;
    private float sentAverage;
    private float receivedAverage;

    public Connection(Channel channel) {
        this.channel = channel;
    }

    public Channel getChannel() {
        return channel;
    }
    public static final Marker ROOT_MARKER = org.slf4j.MarkerFactory.getMarker("NETWORK");
    public static final Marker PACKET_MARKER = packetMarker("NETWORK_PACKETS", ROOT_MARKER);
    public static final Marker PACKET_RECEIVED_MARKER = packetMarker("PACKET_RECEIVED", PACKET_MARKER);
    public static final Marker PACKET_SENT_MARKER = packetMarker("PACKET_SENT", PACKET_MARKER);
    private static Marker packetMarker(String name, Marker parent) {
        Marker marker = org.slf4j.MarkerFactory.getMarker(name);
        marker.add(parent);
        return marker;
    }
    public Connection(PacketFlow receiving) {
        this.receiving = receiving;
    }
    public void channelActive(ChannelHandlerContext ctx) throws Exception {
        super.channelActive(ctx);
    }
    public void channelInactive(ChannelHandlerContext ctx) {
        handleDisconnection();
    }
    public void exceptionCaught(ChannelHandlerContext ctx, Throwable cause) {
        disconnect(Component.literal(String.valueOf(cause)));
    }
    protected void channelRead0(ChannelHandlerContext ctx, Packet<?> packet) {
        receive(packet);
    }
    public <T extends PacketListener> void setupInboundProtocol(ProtocolInfo<T> protocol, T packetListener) {
        this.packetListener = packetListener;
    }
    public void setupOutboundProtocol(ProtocolInfo<?> protocol) {

    }
    public void setListenerForServerboundHandshake(PacketListener packetListener) {
        this.packetListener = packetListener;
    }
    public void initiateServerboundStatusConnection(String hostName, int port, ClientStatusPacketListener listener) {
        initiateServerboundConnection(hostName, port, StatusProtocols.SERVERBOUND, StatusProtocols.CLIENTBOUND, listener, ClientIntent.STATUS);
    }
    public void initiateServerboundPlayConnection(String hostName, int port, ClientLoginPacketListener listener) {
        initiateServerboundConnection(hostName, port, LoginProtocols.SERVERBOUND, LoginProtocols.CLIENTBOUND, listener, ClientIntent.LOGIN);
    }
    public <S extends ServerboundPacketListener, C extends ClientboundPacketListener> void initiateServerboundPlayConnection(String hostName, int port, ProtocolInfo<S> outbound, ProtocolInfo<C> inbound, C listener, boolean transfer) {
        initiateServerboundConnection(hostName, port, outbound, inbound, listener, transfer ? ClientIntent.TRANSFER : ClientIntent.LOGIN);
    }
    private <S extends ServerboundPacketListener, C extends ClientboundPacketListener> void initiateServerboundConnection(String hostName, int port, ProtocolInfo<S> outbound, ProtocolInfo<C> inbound, C listener, ClientIntent intent) {
        if (outbound.id() != inbound.id()) {
            throw new IllegalStateException("Mismatched initial protocols");
        }
        runOnceConnected(connection -> {
            connection.setupInboundProtocol(inbound, listener);
            connection.send(new ClientIntentionPacket(SharedConstants.getProtocolVersion(), hostName, port, intent));
            connection.setupOutboundProtocol(outbound);
        });
    }
    @SuppressWarnings({"unchecked", "rawtypes"})
    public void receive(Packet<?> packet) {
        receivedInWindow++;
        if (packetListener != null && isConnected()) {
            ((Packet) packet).handle(packetListener);
        }
    }
    public void send(Packet<?> packet) {
        send(packet, null, true);
    }
    public void send(Packet<?> packet, ChannelFutureListener listener) {
        send(packet, listener, true);
    }
    public void send(Packet<?> packet, ChannelFutureListener listener, boolean flush) {
        sentInWindow++;
        if (channel == null) {
            return;
        }
        io.netty.channel.ChannelFuture future = flush ? channel.writeAndFlush(packet) : channel.write(packet);
        if (listener != null) {
            future.addListener(listener);
        }
    }
    public void runOnceConnected(Consumer<Connection> action) {
        action.accept(this);
    }
    public void flushChannel() {
        if (channel != null) {
            channel.flush();
        }
    }
    private int ticks;
    public void tick() {
        if (++ticks % 20 == 0) {
            tickSecond();
        }
    }
    protected void tickSecond() {
    }
    public SocketAddress getRemoteAddress() {
        return channel == null ? null : channel.remoteAddress();
    }
    public String getLoggableAddress(boolean logIPs) {
        return logIPs ? String.valueOf(getRemoteAddress()) : "IP hidden";
    }
    public void disconnect(Component reason) {
        disconnect(new DisconnectionDetails(reason));
    }
    public void disconnect(DisconnectionDetails details) {
        if (disconnectionDetails == null) {
            disconnectionDetails = details;
        }
        if (channel != null) {
            channel.close();
        }
    }
    public boolean isMemoryConnection() {
        return false;
    }
    public PacketFlow getReceiving() {
        return receiving;
    }
    public PacketFlow getSending() {
        return receiving.getOpposite();
    }
    public static Connection connectToServer(InetSocketAddress address, EventLoopGroupHolder eventLoopGroupHolder, LocalSampleLogger bandwidthLogger) {
        // Needs a real io.netty Bootstrap and event loop, which Basalt does not run.
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.Connection#connectToServer");
    }
    public static ChannelFuture connect(InetSocketAddress address, EventLoopGroupHolder eventLoopGroupHolder, Connection connection) {
        // io.netty Bootstrap only.
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.Connection#connect");
    }
    public void configurePacketHandler(ChannelPipeline pipeline) {

    }
    public static void configureSerialization(ChannelPipeline pipeline, PacketFlow inboundDirection, boolean local, BandwidthDebugMonitor monitor) {

    }
    public static void configureInMemoryPipeline(ChannelPipeline pipeline, PacketFlow packetFlow) {

    }
    public static Connection connectToLocalServer(SocketAddress address) {
        // io.netty LocalChannel only.
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.Connection#connectToLocalServer");
    }
    public static Connection fromChannel(Channel channel, PacketFlow flow, LocalSampleLogger bandwidthLogger) {
        Connection connection = new Connection(channel);
        connection.receiving = flow;
        return connection;
    }
    public void setEncryptionKey(Cipher decryptCipher, Cipher encryptCipher) {

    }
    public boolean isConnected() {
        return channel != null && channel.isOpen();
    }
    public boolean isConnecting() {
        return channel == null;
    }
    public PacketListener getPacketListener() {
        return packetListener;
    }
    public DisconnectionDetails getDisconnectionDetails() {
        return disconnectionDetails;
    }
    public void setReadOnly() {
        if (channel != null) {
            channel.config().setAutoRead(false);
        }
    }
    public void setupCompression(int threshold, boolean validateDecompressed) {

    }
    public void handleDisconnection() {
        if (handledDisconnection || packetListener == null) {
            return;
        }
        handledDisconnection = true;
        packetListener.onDisconnect(disconnectionDetails != null ? disconnectionDetails
            : new DisconnectionDetails(Component.literal("Disconnected")));
    }
    public synchronized float getAverageReceivedPackets() {
        rollWindow();
        return receivedAverage;
    }
    public synchronized float getAverageSentPackets() {
        rollWindow();
        return sentAverage;
    }
    private void rollWindow() {
        long now = System.nanoTime();
        for (; now - windowStart >= 1_000_000_000L; windowStart += 1_000_000_000L) {
            sentAverage = sentAverage * 0.75F + sentInWindow * 0.25F;
            receivedAverage = receivedAverage * 0.75F + receivedInWindow * 0.25F;
            sentInWindow = 0;
            receivedInWindow = 0;
        }
    }
    public void setBandwidthLogger(LocalSampleLogger bandwidthLogger) {

    }
}
