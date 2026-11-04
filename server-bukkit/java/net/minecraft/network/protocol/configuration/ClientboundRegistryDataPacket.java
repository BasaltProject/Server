// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.configuration;
import java.util.List;
import net.minecraft.core.Registry;
import net.minecraft.core.RegistrySynchronization;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.resources.ResourceKey;
public record ClientboundRegistryDataPacket(ResourceKey<? extends Registry<?>> registry, List<RegistrySynchronization.PackedRegistryEntry> entries) implements Packet<ClientConfigurationPacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ClientboundRegistryDataPacket> STREAM_CODEC = StreamCodec.of(
        (buf, packet) -> {
            buf.writeIdentifier(packet.registry().identifier());
            buf.writeVarInt(packet.entries().size());
            for (RegistrySynchronization.PackedRegistryEntry entry : packet.entries()) {
                RegistrySynchronization.PackedRegistryEntry.STREAM_CODEC.encode(buf, entry);
            }
        },
        buf -> {
            net.minecraft.resources.ResourceKey<? extends net.minecraft.core.Registry<?>> registry =
                net.minecraft.resources.ResourceKey.createRegistryKey(buf.readIdentifier());
            int count = buf.readVarInt();
            java.util.List<RegistrySynchronization.PackedRegistryEntry> entries = new java.util.ArrayList<>(count);
            for (int i = 0; i < count; i++) {
                entries.add(RegistrySynchronization.PackedRegistryEntry.STREAM_CODEC.decode(buf));
            }
            return new ClientboundRegistryDataPacket(registry, entries);
        });
    public PacketType<ClientboundRegistryDataPacket> type() {
        return ConfigurationPacketTypes.CLIENTBOUND_REGISTRY_DATA;
    }
    public void handle(ClientConfigurationPacketListener listener) {
        listener.handleRegistryData(this);
    }
}
