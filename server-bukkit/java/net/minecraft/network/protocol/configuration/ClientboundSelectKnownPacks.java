// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.configuration;
import net.minecraft.network.codec.ByteBufCodecs;
import io.netty.buffer.ByteBuf;
import java.util.List;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.server.packs.repository.KnownPack;
public record ClientboundSelectKnownPacks(List<KnownPack> knownPacks) implements Packet<ClientConfigurationPacketListener> {
    private static final int kMaxKnownPacks = 64;
    public static final StreamCodec<ByteBuf, ClientboundSelectKnownPacks> STREAM_CODEC = KnownPack.STREAM_CODEC.apply(ByteBufCodecs.list(kMaxKnownPacks)).map(ClientboundSelectKnownPacks::new, ClientboundSelectKnownPacks::knownPacks);
    public PacketType<ClientboundSelectKnownPacks> type() {
        return ConfigurationPacketTypes.CLIENTBOUND_SELECT_KNOWN_PACKS;
    }
    public void handle(ClientConfigurationPacketListener listener) {
        listener.handleSelectKnownPacks(this);
    }
}
