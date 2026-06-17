package org.basaltmc.bukkit;

import java.net.InetAddress;
import java.util.HashSet;
import net.kyori.adventure.text.Component;
import net.kyori.adventure.text.serializer.plain.PlainTextComponentSerializer;
import org.bukkit.command.CommandSender;
import org.bukkit.event.server.ServerListPingEvent;

/** The server events Basalt raises, as the Bukkit and Paper events plugins listen for. */
final class ServerEvents {
    private ServerEvents() {}

    static void install() {
        EventBridge.map("server.ping", ServerListPingEvent.class, f -> {
            String text = f.text("address");
            int colon = text.lastIndexOf(':');
            String host = colon > 0 && text.indexOf(':') == colon ? text.substring(0, colon) : text;
            InetAddress address;
            try {
                address = InetAddress.getByName(host.isEmpty() ? "127.0.0.1" : host);
            } catch (java.io.IOException unresolved) {
                address = InetAddress.getLoopbackAddress();
            }
            return new ServerListPingEvent(address, Component.text(f.text("motd")), (int) f.whole("online"),
                (int) f.whole("max_players"));
        }, (e, f) -> {
            ServerListPingEvent ping = (ServerListPingEvent) e;
            f.put("motd", PlainTextComponentSerializer.plainText().serialize(ping.motd()));
            f.put("max_players", (long) ping.getMaxPlayers());
        });
        EventBridge.map("server.broadcast", org.bukkit.event.server.BroadcastMessageEvent.class, f -> {
            HashSet<CommandSender> everyone = new HashSet<>(BasaltServer.instance().getOnlinePlayers());
            everyone.add(BasaltServer.instance().consoleImpl());
            return new org.bukkit.event.server.BroadcastMessageEvent(false, Component.text(f.text("message")),
                everyone);
        }, (e, f) -> f.put("message", PlainTextComponentSerializer.plainText().serialize(
            ((org.bukkit.event.server.BroadcastMessageEvent) e).message())));
        EventBridge.map("server.command", org.bukkit.event.server.RemoteServerCommandEvent.class,
            f -> f.text("source").equals("rcon") ? new org.bukkit.event.server.RemoteServerCommandEvent(
                BasaltServer.instance().consoleImpl(), f.text("command")) : null,
            (e, f) -> f.put("command", ((org.bukkit.event.server.ServerCommandEvent) e).getCommand()));
        EventBridge.map("server.tick_end", com.destroystokyo.paper.event.server.ServerTickEndEvent.class,
            f -> new com.destroystokyo.paper.event.server.ServerTickEndEvent((int) f.whole("tick"), f.real("duration_ms"),
                (long) (f.real("remaining_ms") * 1_000_000.0)));
        EventBridge.map("server.tick", com.destroystokyo.paper.event.server.ServerTickStartEvent.class,
            f -> new com.destroystokyo.paper.event.server.ServerTickStartEvent((int) f.whole("tick")));
    }
}
