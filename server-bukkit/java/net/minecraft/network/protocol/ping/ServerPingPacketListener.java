// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.ping;

import net.minecraft.network.PacketListener;
public interface ServerPingPacketListener extends PacketListener {
    default void handlePingRequest(ServerboundPingRequestPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.ping.ServerPingPacketListener#handlePingRequest");
    }
}
