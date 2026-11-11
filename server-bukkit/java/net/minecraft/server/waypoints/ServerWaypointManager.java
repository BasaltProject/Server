// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.waypoints;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedHashSet;
import java.util.Map;
import java.util.Set;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.level.gamerules.GameRules;
import net.minecraft.world.waypoints.WaypointManager;
import net.minecraft.world.waypoints.WaypointTransmitter;

public class ServerWaypointManager implements WaypointManager<WaypointTransmitter> {
    private final Set<WaypointTransmitter> transmitters = new LinkedHashSet<>();
    private final Set<ServerPlayer> receivers = new LinkedHashSet<>();
    private final Map<ServerPlayer, Map<WaypointTransmitter, WaypointTransmitter.Connection>> links = new HashMap<>();

    public void trackWaypoint(WaypointTransmitter waypoint) {
        transmitters.add(waypoint);
        for (ServerPlayer player : new ArrayList<>(receivers)) {
            link(player, waypoint);
        }
    }

    public void updateWaypoint(WaypointTransmitter waypoint) {
        if (!transmitters.contains(waypoint)) {
            return;
        }
        for (ServerPlayer player : new ArrayList<>(receivers)) {
            refresh(player, waypoint);
        }
    }

    public void untrackWaypoint(WaypointTransmitter waypoint) {
        for (Map<WaypointTransmitter, WaypointTransmitter.Connection> row : links.values()) {
            WaypointTransmitter.Connection connection = row.remove(waypoint);
            if (connection != null) {
                connection.disconnect();
            }
        }
        transmitters.remove(waypoint);
    }

    public void addPlayer(ServerPlayer player) {
        receivers.add(player);
        for (WaypointTransmitter waypoint : new ArrayList<>(transmitters)) {
            link(player, waypoint);
        }
        if (player.isTransmittingWaypoint()) {
            trackWaypoint(player);
        }
    }

    public void updatePlayer(ServerPlayer player) {
        for (WaypointTransmitter waypoint : new ArrayList<>(transmitters)) {
            refresh(player, waypoint);
        }
    }

    public void removePlayer(ServerPlayer player) {
        Map<WaypointTransmitter, WaypointTransmitter.Connection> row = links.remove(player);
        if (row != null) {
            row.values().forEach(WaypointTransmitter.Connection::disconnect);
        }
        untrackWaypoint(player);
        receivers.remove(player);
    }

    public void breakAllConnections() {
        for (Map<WaypointTransmitter, WaypointTransmitter.Connection> row : links.values()) {
            row.values().forEach(WaypointTransmitter.Connection::disconnect);
        }
        links.clear();
    }

    public void remakeConnections(WaypointTransmitter waypoint) {
        for (ServerPlayer player : new ArrayList<>(receivers)) {
            link(player, waypoint);
        }
    }

    public Set<WaypointTransmitter> transmitters() {
        return transmitters;
    }

    private static boolean locatorBar(ServerPlayer player) {
        return player.level().getGameRules().get(GameRules.LOCATOR_BAR);
    }

    private Map<WaypointTransmitter, WaypointTransmitter.Connection> row(ServerPlayer player) {
        return links.computeIfAbsent(player, p -> new HashMap<>());
    }

    private void link(ServerPlayer player, WaypointTransmitter waypoint) {
        if (player == waypoint || !locatorBar(player)) {
            return;
        }
        WaypointTransmitter.Connection made = waypoint.makeWaypointConnectionWith(player).orElse(null);
        if (made != null) {
            row(player).put(waypoint, made);
            made.connect();
        } else {
            WaypointTransmitter.Connection old = row(player).remove(waypoint);
            if (old != null) {
                old.disconnect();
            }
        }
    }

    private void refresh(ServerPlayer player, WaypointTransmitter waypoint) {
        if (player == waypoint || !locatorBar(player)) {
            return;
        }
        WaypointTransmitter.Connection current = row(player).get(waypoint);
        if (current == null) {
            link(player, waypoint);
        } else if (!current.isBroken()) {
            current.update();
        } else {
            WaypointTransmitter.Connection next = waypoint.makeWaypointConnectionWith(player).orElse(null);
            if (next != null) {
                next.connect();
                row(player).put(waypoint, next);
            } else {
                current.disconnect();
                row(player).remove(waypoint);
            }
        }
    }
}
