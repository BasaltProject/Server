// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.chat.ComponentSerialization;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public record ClientboundSetTitleTextPacket(Component text) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundSetTitleTextPacket> STREAM_CODEC = net.minecraft.network.chat.ComponentSerialization.TRUSTED_STREAM_CODEC.map(ClientboundSetTitleTextPacket::new, ClientboundSetTitleTextPacket::text);
    public PacketType<ClientboundSetTitleTextPacket> type() {
        return GamePacketTypes.CLIENTBOUND_SET_TITLE_TEXT;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.setTitleText(this);
    }
}
