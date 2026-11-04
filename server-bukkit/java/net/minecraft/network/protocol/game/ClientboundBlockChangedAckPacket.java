// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public record ClientboundBlockChangedAckPacket(int sequence) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ClientboundBlockChangedAckPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundBlockChangedAckPacket", () -> StreamCodec.of((output, packet) -> {
            output.writeVarInt(packet.sequence());
        }, input -> new ClientboundBlockChangedAckPacket(input.readVarInt())));
    private ClientboundBlockChangedAckPacket(FriendlyByteBuf input) {
        this((int) 0); throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundBlockChangedAckPacket#<init>");
    }
    public PacketType<ClientboundBlockChangedAckPacket> type() {
        return GamePacketTypes.CLIENTBOUND_BLOCK_CHANGED_ACK;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleBlockChangedAck(this);
    }
}
