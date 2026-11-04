// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.network;

import net.minecraft.network.protocol.Packet;
import net.minecraft.server.level.ServerPlayer;
public interface ServerPlayerConnection {
    default ServerPlayer getPlayer() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.network.ServerPlayerConnection#getPlayer");
    }
    default void send(Packet<?> packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.server.network.ServerPlayerConnection#send");
    }
}
