// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public class ClientboundSetChunkCacheCenterPacket implements Packet<ClientGamePacketListener> {
    private final int x;
    private final int z;
    public static final StreamCodec<FriendlyByteBuf, ClientboundSetChunkCacheCenterPacket> STREAM_CODEC = Packet.codec(ClientboundSetChunkCacheCenterPacket::write, ClientboundSetChunkCacheCenterPacket::new);
    public ClientboundSetChunkCacheCenterPacket(int x, int z) {
        this.x = x;
        this.z = z;
    }
    private ClientboundSetChunkCacheCenterPacket(FriendlyByteBuf input) {
        this.x = input.readVarInt();
        this.z = input.readVarInt();
    }
    private void write(FriendlyByteBuf output) {
        output.writeVarInt(this.x);
        output.writeVarInt(this.z);
    }
    public PacketType<ClientboundSetChunkCacheCenterPacket> type() {
        return GamePacketTypes.CLIENTBOUND_SET_CHUNK_CACHE_CENTER;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleSetChunkCacheCenter(this);
    }
    public int getX() {
        return x;
    }
    public int getZ() {
        return z;
    }
}
