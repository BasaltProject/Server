// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

import java.util.HashMap;
import java.util.Map;
import java.util.Set;

public final class PlayerMap {
    private final Map<ServerPlayer, Boolean> players = new HashMap<>();

    public Set<ServerPlayer> getAllPlayers() {
        return players.keySet();
    }

    public void addPlayer(ServerPlayer player, boolean ignored) {
        players.put(player, ignored);
    }

    public void removePlayer(ServerPlayer player) {
        players.remove(player);
    }

    public void ignorePlayer(ServerPlayer player) {
        players.replace(player, true);
    }

    public void unIgnorePlayer(ServerPlayer player) {
        players.replace(player, false);
    }

    public boolean ignoredOrUnknown(ServerPlayer player) {
        return players.getOrDefault(player, true);
    }

    public boolean ignored(ServerPlayer player) {
        return players.getOrDefault(player, false);
    }
}
