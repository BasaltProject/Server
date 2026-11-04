// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.chat.ComponentSerialization;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public record ClientboundSetSubtitleTextPacket(Component text) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundSetSubtitleTextPacket> STREAM_CODEC = net.minecraft.network.chat.ComponentSerialization.TRUSTED_STREAM_CODEC.map(ClientboundSetSubtitleTextPacket::new, ClientboundSetSubtitleTextPacket::text);
    public PacketType<ClientboundSetSubtitleTextPacket> type() {
        return GamePacketTypes.CLIENTBOUND_SET_SUBTITLE_TEXT;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.setSubtitleText(this);
    }
}
