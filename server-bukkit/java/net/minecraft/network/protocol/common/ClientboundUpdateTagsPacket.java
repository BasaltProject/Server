// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.common;
import java.util.IdentityHashMap;
import net.minecraft.network.codec.ByteBufCodecs;
import io.netty.buffer.ByteBuf;
import java.util.Map;
import net.minecraft.core.Registry;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagNetworkSerialization;
public record ClientboundUpdateTagsPacket(Map<ResourceKey<? extends Registry<?>>, TagNetworkSerialization.NetworkPayload> tags) implements Packet<ClientCommonPacketListener> {
    public static final StreamCodec<ByteBuf, ClientboundUpdateTagsPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundUpdateTagsPacket.STREAM_CODEC", () -> StreamCodec.composite(ByteBufCodecs.map(IdentityHashMap::new, ResourceKey.REGISTRY_STREAM_CODEC, TagNetworkSerialization.NetworkPayload.STREAM_CODEC), ClientboundUpdateTagsPacket::tags, ClientboundUpdateTagsPacket::new));
    public PacketType<ClientboundUpdateTagsPacket> type() {
        return CommonPacketTypes.CLIENTBOUND_UPDATE_TAGS;
    }
    public void handle(ClientCommonPacketListener listener) {
        listener.handleUpdateTags(this);
    }
}
