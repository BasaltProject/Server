// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.core.registries.Registries;
import net.minecraft.network.codec.ByteBufCodecs;
import io.netty.buffer.ByteBuf;
import java.util.List;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.gamerules.GameRule;
public record ServerboundSetGameRulePacket(List<Entry> entries) implements Packet<ServerGamePacketListener> {
    public static final StreamCodec<ByteBuf, ServerboundSetGameRulePacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ServerboundSetGameRulePacket.STREAM_CODEC", () -> StreamCodec.composite(ServerboundSetGameRulePacket.Entry.STREAM_CODEC.apply(ByteBufCodecs.list()), ServerboundSetGameRulePacket::entries, ServerboundSetGameRulePacket::new));
    public PacketType<ServerboundSetGameRulePacket> type() {
        return GamePacketTypes.SERVERBOUND_SET_GAME_RULE;
    }
    public void handle(ServerGamePacketListener listener) {
        listener.handleSetGameRule(this);
    }
    public static record Entry(ResourceKey<GameRule<?>> gameRuleKey, String value) {
        public static final StreamCodec<ByteBuf, Entry> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ServerboundSetGameRulePacket.Entry.STREAM_CODEC", () -> StreamCodec.composite(ResourceKey.streamCodec(Registries.GAME_RULE), Entry::gameRuleKey, ByteBufCodecs.STRING_UTF8, Entry::value, Entry::new));
    }
}
