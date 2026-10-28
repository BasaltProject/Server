package org.basaltmc.bukkit;

import java.net.InetAddress;
import java.util.HashSet;
import net.kyori.adventure.text.Component;
import net.kyori.adventure.text.serializer.plain.PlainTextComponentSerializer;
import org.bukkit.command.CommandSender;
import org.bukkit.event.server.ServerListPingEvent;

final class ServerEvents {
    private ServerEvents() {}

    static void install() {
        EventBridge.mapAlways("server.data_reload", f -> ServerLeftovers.dropDataCaches());
        EventBridge.map("server.ping", com.destroystokyo.paper.event.server.PaperServerListPingEvent.class, f -> {
            String text = f.text("address");
            int colon = text.lastIndexOf(':');
            String host = colon > 0 && text.indexOf(':') == colon ? text.substring(0, colon) : text;
            InetAddress address;
            try {
                address = InetAddress.getByName(host.isEmpty() ? "127.0.0.1" : host);
            } catch (java.io.IOException unresolved) {
                address = InetAddress.getLoopbackAddress();
            }
            java.net.InetSocketAddress remote = new java.net.InetSocketAddress(address, 0);
            com.destroystokyo.paper.network.StatusClient client = new com.destroystokyo.paper.network.StatusClient() {
                @Override
                public java.net.SocketAddress getSocketAddress() {
                    return remote;
                }

                @Override
                public java.net.InetSocketAddress getAddress() {
                    return remote;
                }

                @Override
                public int getProtocolVersion() {
                    return 777;
                }

                @Override
                public java.net.InetSocketAddress getVirtualHost() {
                    return null;
                }
            };
            return new com.destroystokyo.paper.event.server.PaperServerListPingEvent(client,
                Component.text(f.text("motd")), (int) f.whole("online"), (int) f.whole("max_players"),
                BasaltServer.instance().getMinecraftVersion(), 777, null);
        }, (e, f) -> {
            ServerListPingEvent ping = (ServerListPingEvent) e;
            f.put("motd", net.kyori.adventure.text.minimessage.MiniMessage.miniMessage().serialize(ping.motd()));
            f.put("max_players", (long) ping.getMaxPlayers());
        });
        EventBridge.map("server.broadcast", org.bukkit.event.server.BroadcastMessageEvent.class, f -> {
            HashSet<CommandSender> everyone = new HashSet<>(BasaltServer.instance().getOnlinePlayers());
            everyone.add(BasaltServer.instance().consoleImpl());
            return new org.bukkit.event.server.BroadcastMessageEvent(false, Component.text(f.text("message")),
                everyone);
        }, (e, f) -> f.put("message", PlainTextComponentSerializer.plainText().serialize(
            ((org.bukkit.event.server.BroadcastMessageEvent) e).message())));
        EventBridge.map("server.remote_command", org.bukkit.event.server.RemoteServerCommandEvent.class,
            f -> new org.bukkit.event.server.RemoteServerCommandEvent(
                new BasaltRemoteConsole(f.text("address")), f.text("command")),
            (e, f) -> f.put("command", ((org.bukkit.event.server.ServerCommandEvent) e).getCommand()));
        EventBridge.map("server.tick_end", com.destroystokyo.paper.event.server.ServerTickEndEvent.class,
            f -> new com.destroystokyo.paper.event.server.ServerTickEndEvent((int) f.whole("tick"), f.real("duration_ms"),
                (long) (f.real("remaining_ms") * 1_000_000.0)));
        EventBridge.map("server.tick", com.destroystokyo.paper.event.server.ServerTickStartEvent.class,
            f -> new com.destroystokyo.paper.event.server.ServerTickStartEvent((int) f.whole("tick")));
    }
}
