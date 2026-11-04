// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol;
import java.util.ArrayList;
import java.util.List;
import java.util.function.Consumer;
import java.util.function.Function;
import net.minecraft.network.PacketListener;
public interface BundlerInfo {
    int BUNDLE_SIZE_LIMIT = 4096;
    static <T extends PacketListener, P extends BundlePacket<? super T>> BundlerInfo createForPacket(PacketType<P> bundlePacketType, Function<Iterable<Packet<? super T>>, P> constructor, BundleDelimiterPacket<? super T> delimiterPacket) {
        return new BundlerInfo() {
            public void unbundlePacket(Packet<?> packet, Consumer<Packet<?>> output) {
                if (packet.type() == bundlePacketType) {
                    @SuppressWarnings("unchecked")
                    P bundle = (P) (BundlePacket<?>) packet;
                    output.accept(delimiterPacket);
                    bundle.subPackets().forEach(output);
                    output.accept(delimiterPacket);
                } else {
                    output.accept(packet);
                }
            }
            public Bundler startPacketBundling(Packet<?> first) {
                if (first != delimiterPacket) {
                    return null;
                }
                List<Packet<? super T>> collected = new ArrayList<>();
                return incoming -> {
                    if (incoming == delimiterPacket) {
                        return constructor.apply(collected);
                    }
                    if (collected.size() >= BUNDLE_SIZE_LIMIT) {
                        throw new IllegalStateException("Too many packets in a bundle");
                    }
                    @SuppressWarnings("unchecked")
                    Packet<? super T> typed = (Packet<? super T>) incoming;
                    collected.add(typed);
                    return null;
                };
            }
        };
    }
    void unbundlePacket(Packet<?> packet, Consumer<Packet<?>> output);
    Bundler startPacketBundling(Packet<?> packet);
    public interface Bundler {
        Packet<?> addPacket(Packet<?> packet);
    }
}
