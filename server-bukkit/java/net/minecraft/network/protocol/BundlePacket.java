// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol;

import net.minecraft.network.PacketListener;
public abstract class BundlePacket<T extends PacketListener> implements Packet<T> {
    private final Iterable<Packet<? super T>> packets;
    protected BundlePacket(Iterable<Packet<? super T>> packets) {
        this.packets = java.util.List.copyOf(toList(packets));
    }
    private static <T> java.util.List<Packet<? super T>> toList(Iterable<Packet<? super T>> packets) {
        java.util.List<Packet<? super T>> result = new java.util.ArrayList<>();
        packets.forEach(result::add);
        return result;
    }
    public Iterable<Packet<? super T>> subPackets() {
        return packets;
    }
    public abstract PacketType<? extends BundlePacket<T>> type();
}
