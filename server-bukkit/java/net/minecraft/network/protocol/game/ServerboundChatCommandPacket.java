// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public record ServerboundChatCommandPacket(String command) implements Packet<ServerGamePacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ServerboundChatCommandPacket> STREAM_CODEC = StreamCodec.of((buf, p) -> buf.writeUtf(p.command(), 32767), buf -> new ServerboundChatCommandPacket(buf.readUtf(32767)));
    public PacketType<ServerboundChatCommandPacket> type() {
        return GamePacketTypes.SERVERBOUND_CHAT_COMMAND;
    }
    public void handle(ServerGamePacketListener listener) {
        listener.handleChatCommand(this);
    }
}
