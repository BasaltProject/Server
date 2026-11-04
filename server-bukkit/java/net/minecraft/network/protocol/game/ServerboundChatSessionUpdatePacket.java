// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.codec.ByteBufCodecs;
import io.netty.buffer.ByteBuf;
import net.minecraft.network.chat.RemoteChatSession;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public record ServerboundChatSessionUpdatePacket(RemoteChatSession.Data chatSession) implements Packet<ServerGamePacketListener> {
    public static final StreamCodec<ByteBuf, ServerboundChatSessionUpdatePacket> STREAM_CODEC = net.minecraft.network.chat.RemoteChatSession.Data.STREAM_CODEC.map(ServerboundChatSessionUpdatePacket::new, ServerboundChatSessionUpdatePacket::chatSession);
    public PacketType<ServerboundChatSessionUpdatePacket> type() {
        return GamePacketTypes.SERVERBOUND_CHAT_SESSION_UPDATE;
    }
    public void handle(ServerGamePacketListener listener) {
        listener.handleChatSessionUpdate(this);
    }
}
