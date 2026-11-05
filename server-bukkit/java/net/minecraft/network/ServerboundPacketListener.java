// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network;

import net.minecraft.network.protocol.PacketFlow;
public interface ServerboundPacketListener extends PacketListener {
    default PacketFlow flow() {
        return PacketFlow.SERVERBOUND;
    }
}
