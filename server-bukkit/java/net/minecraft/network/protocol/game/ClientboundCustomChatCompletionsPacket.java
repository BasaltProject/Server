// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import java.util.function.IntFunction;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.util.ByIdMap;
import io.netty.buffer.ByteBuf;
import java.util.List;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public record ClientboundCustomChatCompletionsPacket(Action action, List<String> entries) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<ByteBuf, ClientboundCustomChatCompletionsPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundCustomChatCompletionsPacket.STREAM_CODEC", () -> StreamCodec.composite(ClientboundCustomChatCompletionsPacket.Action.STREAM_CODEC, ClientboundCustomChatCompletionsPacket::action, ByteBufCodecs.STRING_UTF8.apply(ByteBufCodecs.list()), ClientboundCustomChatCompletionsPacket::entries, ClientboundCustomChatCompletionsPacket::new));
    public PacketType<ClientboundCustomChatCompletionsPacket> type() {
        return GamePacketTypes.CLIENTBOUND_CUSTOM_CHAT_COMPLETIONS;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleCustomChatCompletions(this);
    }
    public static enum Action {
        ADD, REMOVE, SET;
        public static final StreamCodec<ByteBuf, Action> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.network.protocol.game.ClientboundCustomChatCompletionsPacket.Action.class, "STREAM_CODEC");
    }
}
