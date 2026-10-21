package org.basaltmc.bukkit.net;

import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import io.netty.channel.ChannelFuture;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.ChannelInboundHandlerAdapter;
import io.netty.channel.ChannelOutboundHandlerAdapter;
import io.netty.channel.ChannelPromise;
import io.netty.channel.DefaultEventLoopGroup;
import io.netty.channel.EventLoop;
import java.net.InetSocketAddress;
import java.util.ArrayList;
import java.util.List;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import net.minecraft.network.Connection;
import org.basaltmc.bukkit.Natives;

public final class ChannelBridge {
    private static final int kHandshake = 0;
    private static final int kServerProtocol = 777;
    private static final long kInboundTimeoutSeconds = 5;
    private static final boolean kTrace = System.getenv("BASALT_PACKET_TRACE") != null;

    private static final ConcurrentHashMap<Long, Link> links = new ConcurrentHashMap<>();
    private static final Object guard = new Object();
    private static DefaultEventLoopGroup group;
    private static BasaltServerChannel serverChannel;
    private static List<ChannelFuture> serverChannels;
    private static List<Connection> connections;
    private static boolean tapped;

    private ChannelBridge() {}

    public static final class ServerChannels extends ArrayList<ChannelFuture> {
        private static final long serialVersionUID = 1L;

        @Override
        public java.util.Iterator<ChannelFuture> iterator() {
            start();
            return super.iterator();
        }
    }

    private static final class Link {
        final BasaltChannel channel;
        final Connection connection;
        volatile Pending pending;

        Link(BasaltChannel channel) {
            this.channel = channel;
            this.connection = new Connection(channel);
        }
    }

    private static final class Pending {
        volatile byte[] result;
        volatile boolean reached;
    }

    public static void attach(List<ChannelFuture> channels, List<Connection> live) {
        serverChannels = channels;
        connections = live;
    }

    private static void start() {
        synchronized (guard) {
            if (tapped) {
                return;
            }
            tapped = true;
            group = new DefaultEventLoopGroup(2);
            serverChannel = new BasaltServerChannel();
            group.next().register(serverChannel).syncUninterruptibly();
            ChannelFuture future = serverChannel.newSucceededFuture();
            // Added to the backing store directly: the iterator that started us must not start us again.
            ArrayList<ChannelFuture> store = (ArrayList<ChannelFuture>) serverChannels;
            store.add(future);
            Natives.packetTap(true);
        }
    }

    public static BasaltChannel channelOf(UUID player) {
        long id = Natives.packetConnectionOf(player.toString());
        Link link = id == 0 ? null : links.get(id);
        if (kTrace) {
            Natives.log(1, "PacketTrace", "channelOf " + player + " connection " + id + (link == null ? " none" : " found"));
        }
        return link == null ? null : link.channel;
    }

    private static int readVarInt(byte[] data) {
        int value = 0;
        int shift = 0;
        for (int i = 0; i < data.length && i < 5; i++) {
            value |= (data[i] & 0x7F) << shift;
            if ((data[i] & 0x80) == 0) {
                return value;
            }
            shift += 7;
        }
        return -1;
    }

    private static void writeVarInt(ByteBuf out, int value) {
        while ((value & ~0x7F) != 0) {
            out.writeByte((value & 0x7F) | 0x80);
            value >>>= 7;
        }
        out.writeByte(value);
    }

    private static int varIntLength(int value) {
        int length = 1;
        while ((value & ~0x7F) != 0) {
            length++;
            value >>>= 7;
        }
        return length;
    }

    private static final class FromWire extends ChannelInboundHandlerAdapter {
        private final Link link;

        FromWire(Link link) {
            this.link = link;
        }

        @Override
        public void channelRead(ChannelHandlerContext ctx, Object message) {
            ByteBuf buffer = (ByteBuf) message;
            try {
                Pending pending = link.pending;
                if (pending == null) {
                    // A plugin handed a packet to the server as if the client had sent it.
                    if (buffer.isReadable()) {
                        byte[] whole = new byte[buffer.readableBytes()];
                        buffer.readBytes(whole);
                        int id = readVarInt(whole);
                        int skip = varIntLength(id);
                        byte[] body = new byte[whole.length - skip];
                        System.arraycopy(whole, skip, body, 0, body.length);
                        Natives.packetReceive(link.channel.connection(), id, body);
                    }
                    return;
                }
                pending.reached = true;
                if (buffer.isReadable()) {
                    byte[] whole = new byte[buffer.readableBytes()];
                    buffer.readBytes(whole);
                    pending.result = whole;
                }
            } finally {
                buffer.release();
            }
        }
    }

    private static final class ToWire extends ChannelOutboundHandlerAdapter {
        private final Link link;

        ToWire(Link link) {
            this.link = link;
        }

        @Override
        public void write(ChannelHandlerContext ctx, Object message, ChannelPromise promise) {
            if (!(message instanceof ByteBuf)) {
                ctx.write(message, promise);
                return;
            }
            ByteBuf buffer = (ByteBuf) message;
            try {
                if (buffer.isReadable()) {
                    byte[] whole = new byte[buffer.readableBytes()];
                    buffer.readBytes(whole);
                    int id = readVarInt(whole);
                    int skip = varIntLength(id);
                    byte[] body = new byte[whole.length - skip];
                    System.arraycopy(whole, skip, body, 0, body.length);
                    Natives.packetSend(link.channel.connection(), id, body);
                }
                promise.setSuccess();
            } finally {
                buffer.release();
            }
        }
    }

    private static Link open(long connection, String address, int port) {
        Link made;
        synchronized (guard) {
            if (serverChannel == null) {
                return null;
            }
            InetSocketAddress remote = new InetSocketAddress(address, port);
            BasaltChannel channel = new BasaltChannel(connection, remote, new InetSocketAddress("0.0.0.0", 25565));
            made = new Link(channel);
            EventLoop loop = group.next();
            final Link fresh = made;
            fresh.channel.pipeline().addLast("splitter", new ChannelInboundHandlerAdapter());
            fresh.channel.pipeline().addLast("decoder", new FromWire(fresh));
            fresh.channel.pipeline().addLast("prepender", new ToWire(fresh));
            fresh.channel.pipeline().addLast("encoder", new ChannelOutboundHandlerAdapter());
            links.put(connection, made);
            try {
                // Like netty's acceptor: the server pipeline sees the child first, then the child registers.
                serverChannel.eventLoop().submit(() -> serverChannel.pipeline().fireChannelRead(fresh.channel))
                    .get(kInboundTimeoutSeconds, TimeUnit.SECONDS);
                loop.register(fresh.channel).get(kInboundTimeoutSeconds, TimeUnit.SECONDS);
                loop.submit(() -> {}).get(kInboundTimeoutSeconds, TimeUnit.SECONDS);
            } catch (InterruptedException | ExecutionException | TimeoutException e) {
                Natives.log(3, "Bukkit", "Could not open a channel for connection " + connection + ": " + e);
                links.remove(connection);
                return null;
            }
            if (kTrace) {
                Natives.log(1, "PacketTrace", "opened " + connection + " pipeline " + made.channel.pipeline().names()
                    + " server pipeline " + serverChannel.pipeline().names());
            }
            connections.add(made.connection);
        }
        return made;
    }

    private static byte[] patchHandshake(byte[] body) {
        int version = readVarInt(body);
        if (version == kServerProtocol || version < 0) {
            return body;
        }
        int skip = varIntLength(version);
        ByteBuf out = Unpooled.buffer(body.length + 2);
        writeVarInt(out, kServerProtocol);
        out.writeBytes(body, skip, body.length - skip);
        byte[] patched = new byte[out.readableBytes()];
        out.readBytes(patched);
        out.release();
        return patched;
    }

    private static ByteBuf frame(int id, byte[] body) {
        ByteBuf buffer = Unpooled.buffer(body.length + 5);
        writeVarInt(buffer, id);
        buffer.writeBytes(body);
        return buffer;
    }

    private static Link linkFor(long connection, int phase, String address, int port) {
        Link link = links.get(connection);
        if (link == null && phase == kHandshake) {
            link = open(connection, address, port);
        }
        return link;
    }

    /** A serverbound packet; returns the same array to let it through, null to drop it, another array to rewrite it. */
    public static byte[] inbound(long connection, int phase, int id, byte[] body, String address, int port) {
        Link link = linkFor(connection, phase, address, port);
        if (kTrace) {
            Natives.log(1, "PacketTrace", "in conn " + connection + " phase " + phase + " id " + id + " len " + body.length
                + (link == null ? " unmanaged" : ""));
        }
        if (link == null) {
            return body;
        }
        byte[] shown = phase == kHandshake && id == 0 ? patchHandshake(body) : body;
        Pending pending = new Pending();
        Future<?> done = link.channel.eventLoop().submit(() -> {
            link.pending = pending;
            try {
                link.channel.pipeline().fireChannelRead(frame(id, shown));
            } catch (Throwable t) {
                Natives.log(2, "Bukkit", "A packet listener failed on an inbound packet: " + t);
            } finally {
                link.pending = null;
            }
        });
        try {
            done.get(kInboundTimeoutSeconds, TimeUnit.SECONDS);
        } catch (InterruptedException | ExecutionException | TimeoutException e) {
            Natives.log(2, "Bukkit", "An inbound packet took too long in a packet listener: " + e);
            return body;
        }
        if (!pending.reached || pending.result == null) {
            return null;
        }
        byte[] result = pending.result;
        int newId = readVarInt(result);
        int skip = varIntLength(newId);
        if (newId != id) {
            byte[] fresh = new byte[result.length - skip];
            System.arraycopy(result, skip, fresh, 0, fresh.length);
            Natives.packetReceive(connection, newId, fresh);
            return null;
        }
        if (phase == kHandshake) {
            return body;
        }
        int length = result.length - skip;
        if (length == body.length && java.util.Arrays.equals(java.util.Arrays.copyOfRange(result, skip, result.length), body)) {
            return body;
        }
        byte[] fresh = new byte[length];
        System.arraycopy(result, skip, fresh, 0, length);
        return fresh;
    }

    /** A clientbound packet; true when a plugin's pipeline took it over and will send what comes out of it. */
    public static boolean outbound(long connection, int phase, int id, byte[] body) {
        Link link = links.get(connection);
        if (kTrace) {
            Natives.log(1, "PacketTrace", "out conn " + connection + " phase " + phase + " id " + id + " len " + body.length
                + (link == null ? " unmanaged" : ""));
        }
        if (link == null) {
            return false;
        }
        link.channel.eventLoop().execute(() -> {
            try {
                link.channel.writeAndFlush(frame(id, body));
            } catch (Throwable t) {
                Natives.log(2, "Bukkit", "A packet listener failed on an outbound packet: " + t);
            }
        });
        return true;
    }

    public static void closed(long connection) {
        Link link = links.remove(connection);
        if (link == null) {
            return;
        }
        connections.remove(link.connection);
        link.channel.markClosedFromNative();
        link.channel.eventLoop().execute(() -> link.channel.unsafe().close(link.channel.unsafe().voidPromise()));
    }

    static void closedByPlugin(long connection) {
        Natives.packetClose(connection);
    }
}
