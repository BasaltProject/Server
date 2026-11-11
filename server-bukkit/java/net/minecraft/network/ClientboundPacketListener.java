// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network;
import net.minecraft.network.protocol.PacketFlow;
public interface ClientboundPacketListener extends PacketListener {
    default PacketFlow flow() {
        return PacketFlow.CLIENTBOUND;
    }
}
