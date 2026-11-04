// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import it.unimi.dsi.fastutil.objects.Object2IntMap;
import it.unimi.dsi.fastutil.objects.Object2IntOpenHashMap;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.stats.Stat;
public record ClientboundAwardStatsPacket(Object2IntMap<Stat<?>> stats) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundAwardStatsPacket> STREAM_CODEC = StreamCodec.composite(net.minecraft.network.codec.ByteBufCodecs.map(Object2IntOpenHashMap::new, Stat.STREAM_CODEC, net.minecraft.network.codec.ByteBufCodecs.VAR_INT), ClientboundAwardStatsPacket::stats, ClientboundAwardStatsPacket::new);
    public PacketType<ClientboundAwardStatsPacket> type() {
        return GamePacketTypes.CLIENTBOUND_AWARD_STATS;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleAwardStats(this);
    }
}
