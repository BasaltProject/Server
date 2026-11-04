// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public class ClientboundTakeItemEntityPacket implements Packet<ClientGamePacketListener> {
    private final int itemId;
    private final int playerId;
    private final int amount;
    public static final StreamCodec<FriendlyByteBuf, ClientboundTakeItemEntityPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundTakeItemEntityPacket", () -> Packet.codec(ClientboundTakeItemEntityPacket::write, ClientboundTakeItemEntityPacket::new));
    public ClientboundTakeItemEntityPacket(int itemId, int playerId, int amount) {
        this.itemId = itemId;
        this.playerId = playerId;
        this.amount = amount;
    }
    private ClientboundTakeItemEntityPacket(FriendlyByteBuf input) {
        this.itemId = input.readVarInt();
        this.playerId = input.readVarInt();
        this.amount = input.readVarInt();
    }
    private void write(FriendlyByteBuf output) {
        output.writeVarInt(this.itemId);
        output.writeVarInt(this.playerId);
        output.writeVarInt(this.amount);
    }
    public PacketType<ClientboundTakeItemEntityPacket> type() {
        return GamePacketTypes.CLIENTBOUND_TAKE_ITEM_ENTITY;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleTakeItemEntity(this);
    }
    public int getItemId() {
        return itemId;
    }
    public int getPlayerId() {
        return playerId;
    }
    public int getAmount() {
        return amount;
    }
}
