// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.configuration;
import net.minecraft.network.codec.ByteBufCodecs;
import io.netty.buffer.ByteBuf;
import java.util.List;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.server.packs.repository.KnownPack;
public record ServerboundSelectKnownPacks(List<KnownPack> knownPacks) implements Packet<ServerConfigurationPacketListener> {
    private static final int kMaxKnownPacks = 64;
    public static final StreamCodec<ByteBuf, ServerboundSelectKnownPacks> STREAM_CODEC = KnownPack.STREAM_CODEC.apply(ByteBufCodecs.list(kMaxKnownPacks)).map(ServerboundSelectKnownPacks::new, ServerboundSelectKnownPacks::knownPacks);
    public PacketType<ServerboundSelectKnownPacks> type() {
        return ConfigurationPacketTypes.SERVERBOUND_SELECT_KNOWN_PACKS;
    }
    public void handle(ServerConfigurationPacketListener listener) {
        listener.handleSelectKnownPacks(this);
    }
}
