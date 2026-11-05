// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.ping;

import net.minecraft.network.PacketListener;
public interface ServerPingPacketListener extends PacketListener {
    void handlePingRequest(ServerboundPingRequestPacket packet);
}
