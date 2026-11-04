// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.codec.ByteBufCodecs;
import io.netty.buffer.ByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public record ServerboundCommandSuggestionPacket(int id, String command) implements Packet<ServerGamePacketListener> {
    public static final StreamCodec<ByteBuf, ServerboundCommandSuggestionPacket> STREAM_CODEC = StreamCodec.composite(ByteBufCodecs.VAR_INT, ServerboundCommandSuggestionPacket::id, ByteBufCodecs.stringUtf8(32500),
            ServerboundCommandSuggestionPacket::command, ServerboundCommandSuggestionPacket::new);
    public static final StreamCodec<ByteBuf, ServerboundCommandSuggestionPacket> CHAT_ONLY_STREAM_CODEC = StreamCodec.composite(ByteBufCodecs.VAR_INT, ServerboundCommandSuggestionPacket::id, ByteBufCodecs.stringUtf8(32500),
            ServerboundCommandSuggestionPacket::command, ServerboundCommandSuggestionPacket::new);
    public PacketType<ServerboundCommandSuggestionPacket> type() {
        return GamePacketTypes.SERVERBOUND_COMMAND_SUGGESTION;
    }
    public void handle(ServerGamePacketListener listener) {
        listener.handleCustomCommandSuggestions(this);
    }
}
