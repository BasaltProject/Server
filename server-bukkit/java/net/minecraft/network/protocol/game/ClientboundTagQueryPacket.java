// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public class ClientboundTagQueryPacket implements Packet<ClientGamePacketListener> {
    private final int transactionId;
    private final CompoundTag tag;
    public static final StreamCodec<FriendlyByteBuf, ClientboundTagQueryPacket> STREAM_CODEC = Packet.codec(ClientboundTagQueryPacket::write, ClientboundTagQueryPacket::new);
    public ClientboundTagQueryPacket(int transactionId, CompoundTag tag) {
        this.transactionId = transactionId;
        this.tag = tag;
    }
    private ClientboundTagQueryPacket(FriendlyByteBuf input) {
        this.transactionId = input.readVarInt();
        this.tag = input.readNbt();
    }
    private void write(FriendlyByteBuf output) {
        output.writeVarInt(this.transactionId);
        output.writeNbt(this.tag);
    }
    public PacketType<ClientboundTagQueryPacket> type() {
        return GamePacketTypes.CLIENTBOUND_TAG_QUERY;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleTagQueryPacket(this);
    }
    public int getTransactionId() {
        return transactionId;
    }
    public CompoundTag getTag() {
        return tag;
    }
    public boolean isSkippable() {
        return true;
    }
}
