// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.codec.ByteBufCodecs;
import io.netty.buffer.ByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public record ClientboundSetHeldSlotPacket(int slot) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<ByteBuf, ClientboundSetHeldSlotPacket> STREAM_CODEC = net.minecraft.network.codec.ByteBufCodecs.VAR_INT.map(ClientboundSetHeldSlotPacket::new, ClientboundSetHeldSlotPacket::slot);
    public PacketType<ClientboundSetHeldSlotPacket> type() {
        return GamePacketTypes.CLIENTBOUND_SET_HELD_SLOT;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleSetHeldSlot(this);
    }
}
