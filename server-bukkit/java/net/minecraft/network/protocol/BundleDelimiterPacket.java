// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol;
import net.minecraft.network.PacketListener;
public abstract class BundleDelimiterPacket<T extends PacketListener> implements Packet<T> {
    public final void handle(T listener) {
        throw new AssertionError("This packet should be handled by pipeline");
    }
    public abstract PacketType<? extends BundleDelimiterPacket<T>> type();
}
