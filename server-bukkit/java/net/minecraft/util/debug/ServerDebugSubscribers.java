// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.debug;

import java.util.Set;
import net.minecraft.network.protocol.Packet;
import net.minecraft.server.MinecraftServer;
import net.minecraft.server.level.ServerPlayer;

public class ServerDebugSubscribers {
    private final MinecraftServer server;

    public ServerDebugSubscribers(MinecraftServer server) {
        this.server = server;
    }

    public void tick() {}

    public void broadcastToAll(DebugSubscription<?> subscription, Packet<?> packet) {}

    public Set<DebugSubscription<?>> enabledSubscriptions() {
        return Set.of();
    }

    public boolean hasAnySubscriberFor(DebugSubscription<?> subscription) {
        return false;
    }

    public boolean hasRequiredPermissions(ServerPlayer player) {
        return player.bukkitPlayer().isOp();
    }
}
