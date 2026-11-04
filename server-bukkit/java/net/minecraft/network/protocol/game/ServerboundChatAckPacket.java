// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public record ServerboundChatAckPacket(int offset) implements Packet<ServerGamePacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ServerboundChatAckPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ServerboundChatAckPacket", () -> StreamCodec.of((output, packet) -> {
            output.writeVarInt(packet.offset());
        }, input -> new ServerboundChatAckPacket(input.readVarInt())));
    private ServerboundChatAckPacket(FriendlyByteBuf input) {
        this((int) 0); throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerboundChatAckPacket#<init>");
    }
    public PacketType<ServerboundChatAckPacket> type() {
        return GamePacketTypes.SERVERBOUND_CHAT_ACK;
    }
    public void handle(ServerGamePacketListener listener) {
        listener.handleChatAck(this);
    }
}
