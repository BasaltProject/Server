// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import it.unimi.dsi.fastutil.ints.IntArrayList;
import net.minecraft.network.codec.ByteBufCodecs;
import io.netty.buffer.ByteBuf;
import it.unimi.dsi.fastutil.ints.IntList;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public record ClientboundRemoveEntitiesPacket(IntList entityIds) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<net.minecraft.network.FriendlyByteBuf, ClientboundRemoveEntitiesPacket> STREAM_CODEC = StreamCodec.of((buf, packet) -> {
            buf.writeVarInt(packet.entityIds().size());
            for (int id : packet.entityIds()) {
                buf.writeVarInt(id);
            }
        }, buf -> {
            int count = buf.readVarInt();
            int[] ids = new int[count];
            for (int i = 0; i < count; i++) {
                ids[i] = buf.readVarInt();
            }
            return new ClientboundRemoveEntitiesPacket(ids);
        });
    public ClientboundRemoveEntitiesPacket(int... ids) {
        this(new it.unimi.dsi.fastutil.ints.IntArrayList(ids));
    }
    public PacketType<ClientboundRemoveEntitiesPacket> type() {
        return GamePacketTypes.CLIENTBOUND_REMOVE_ENTITIES;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleRemoveEntities(this);
    }
}
