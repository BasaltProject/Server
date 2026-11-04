// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.level.ChunkPos;
public record ClientboundForgetLevelChunkPacket(ChunkPos pos) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ClientboundForgetLevelChunkPacket> STREAM_CODEC = StreamCodec.of((buf, p) -> {
            buf.writeInt(p.pos().z());
            buf.writeInt(p.pos().x());
        }, buf -> {
            int z = buf.readInt();
            return new ClientboundForgetLevelChunkPacket(new net.minecraft.world.level.ChunkPos(buf.readInt(), z));
        });
    public PacketType<ClientboundForgetLevelChunkPacket> type() {
        return GamePacketTypes.CLIENTBOUND_FORGET_LEVEL_CHUNK;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleForgetLevelChunk(this);
    }
}
