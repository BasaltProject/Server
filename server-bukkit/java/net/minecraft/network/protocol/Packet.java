// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol;

import io.netty.buffer.ByteBuf;
import net.minecraft.network.PacketListener;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.codec.StreamDecoder;
import net.minecraft.network.codec.StreamMemberEncoder;
public interface Packet<T extends PacketListener> {
    default PacketType<? extends Packet<T>> type() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.Packet#type");
    }
    default void handle(T listener) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.Packet#handle");
    }
    default boolean isSkippable() {
        return false;
    }
    default boolean isTerminal() {
        return false;
    }
    static <B extends ByteBuf, T extends Packet<?>> StreamCodec<B, T> codec(StreamMemberEncoder<B, T> writer, StreamDecoder<B, T> reader) {
        return StreamCodec.of((buf, packet) -> writer.encode(packet, buf), reader);
    }
}
