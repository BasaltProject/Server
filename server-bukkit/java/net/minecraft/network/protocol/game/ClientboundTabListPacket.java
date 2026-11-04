// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.chat.ComponentSerialization;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public record ClientboundTabListPacket(Component header, Component footer) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundTabListPacket> STREAM_CODEC = StreamCodec.composite(net.minecraft.network.chat.ComponentSerialization.TRUSTED_STREAM_CODEC, ClientboundTabListPacket::header, net.minecraft.network.chat.ComponentSerialization.TRUSTED_STREAM_CODEC, ClientboundTabListPacket::footer, ClientboundTabListPacket::new);
    public PacketType<ClientboundTabListPacket> type() {
        return GamePacketTypes.CLIENTBOUND_TAB_LIST;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleTabListCustomisation(this);
    }
}
