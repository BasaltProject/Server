// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.configuration;
import java.util.HashSet;
import net.minecraft.network.codec.ByteBufCodecs;
import io.netty.buffer.ByteBuf;
import java.util.Set;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.resources.Identifier;
public record ClientboundUpdateEnabledFeaturesPacket(Set<Identifier> features) implements Packet<ClientConfigurationPacketListener> {
    public static final StreamCodec<ByteBuf, ClientboundUpdateEnabledFeaturesPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundUpdateEnabledFeaturesPacket.STREAM_CODEC", () -> StreamCodec.composite(Identifier.STREAM_CODEC.apply(ByteBufCodecs.collection(HashSet::new)), ClientboundUpdateEnabledFeaturesPacket::features, ClientboundUpdateEnabledFeaturesPacket::new));
    public PacketType<ClientboundUpdateEnabledFeaturesPacket> type() {
        return ConfigurationPacketTypes.CLIENTBOUND_UPDATE_ENABLED_FEATURES;
    }
    public void handle(ClientConfigurationPacketListener listener) {
        listener.handleEnabledFeatures(this);
    }
}
