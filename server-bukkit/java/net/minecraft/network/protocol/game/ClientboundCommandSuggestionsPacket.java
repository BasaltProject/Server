// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import com.mojang.brigadier.Message;
import com.mojang.brigadier.context.StringRange;
import com.mojang.brigadier.suggestion.Suggestion;
import net.minecraft.network.chat.ComponentSerialization;
import net.minecraft.network.chat.ComponentUtils;
import net.minecraft.network.codec.ByteBufCodecs;
import com.mojang.brigadier.suggestion.Suggestions;
import java.util.List;
import java.util.Optional;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public record ClientboundCommandSuggestionsPacket(int id, int start, int length, List<Entry> suggestions) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundCommandSuggestionsPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundCommandSuggestionsPacket.STREAM_CODEC", () -> StreamCodec.composite(ByteBufCodecs.VAR_INT, ClientboundCommandSuggestionsPacket::id, ByteBufCodecs.VAR_INT, ClientboundCommandSuggestionsPacket::start, ByteBufCodecs.VAR_INT, ClientboundCommandSuggestionsPacket::length, ClientboundCommandSuggestionsPacket.Entry.STREAM_CODEC.apply(ByteBufCodecs.list()), ClientboundCommandSuggestionsPacket::suggestions, ClientboundCommandSuggestionsPacket::new));
    public ClientboundCommandSuggestionsPacket(int id, Suggestions suggestions) {
        this(id, suggestions.getRange().getStart(), suggestions.getRange().getLength(), suggestions.getList().stream()
            .map(each -> new Entry(each.getText(), Optional.ofNullable(each.getTooltip()).map(ComponentUtils::fromMessage))).toList());
    }
    public PacketType<ClientboundCommandSuggestionsPacket> type() {
        return GamePacketTypes.CLIENTBOUND_COMMAND_SUGGESTIONS;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleCommandSuggestions(this);
    }
    public Suggestions toSuggestions() {
        StringRange range = StringRange.between(start, start + length);
        return new Suggestions(range, suggestions.stream()
            .map(each -> new Suggestion(range, each.text(), each.tooltip().orElse(null))).toList());
    }
    public static record Entry(String text, Optional<Component> tooltip) {
        public static final StreamCodec<RegistryFriendlyByteBuf, Entry> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundCommandSuggestionsPacket.Entry.STREAM_CODEC", () -> StreamCodec.composite(ByteBufCodecs.STRING_UTF8, Entry::text, ComponentSerialization.TRUSTED_OPTIONAL_STREAM_CODEC, Entry::tooltip, Entry::new));
    }
}
