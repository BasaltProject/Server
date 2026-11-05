// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network;
public interface TickablePacketListener extends PacketListener {
    void tick();
}
