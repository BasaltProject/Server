// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network;
public interface TickablePacketListener extends PacketListener {
    default void tick() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.TickablePacketListener#tick");
    }
}
