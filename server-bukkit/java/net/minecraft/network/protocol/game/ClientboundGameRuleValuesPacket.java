// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.codec.ByteBufCodecs;
import java.util.Map;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.gamerules.GameRule;
public record ClientboundGameRuleValuesPacket(Map<ResourceKey<GameRule<?>>, String> values) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ClientboundGameRuleValuesPacket> STREAM_CODEC = StreamCodec.of(
        (buf, packet) -> {
            buf.writeVarInt(packet.values().size());
            packet.values().forEach((key, value) -> {
                buf.writeIdentifier(key.identifier());
                buf.writeUtf(value);
            });
        },
        buf -> {
            int count = buf.readVarInt();
            java.util.Map<net.minecraft.resources.ResourceKey<net.minecraft.world.level.gamerules.GameRule<?>>, String> values = new java.util.HashMap<>();
            for (int i = 0; i < count; i++) {
                values.put(net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.GAME_RULE, buf.readIdentifier()), buf.readUtf());
            }
            return new ClientboundGameRuleValuesPacket(values);
        });
    public PacketType<ClientboundGameRuleValuesPacket> type() {
        return GamePacketTypes.CLIENTBOUND_GAME_RULE_VALUES;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleGameRuleValues(this);
    }
}
