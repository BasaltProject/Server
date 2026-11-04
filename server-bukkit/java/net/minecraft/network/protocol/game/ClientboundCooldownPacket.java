// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.resources.Identifier;
public record ClientboundCooldownPacket(Identifier cooldownGroup, int duration) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundCooldownPacket> STREAM_CODEC = StreamCodec.composite(net.minecraft.resources.Identifier.STREAM_CODEC, ClientboundCooldownPacket::cooldownGroup, net.minecraft.network.codec.ByteBufCodecs.VAR_INT, ClientboundCooldownPacket::duration, ClientboundCooldownPacket::new);
    public PacketType<ClientboundCooldownPacket> type() {
        return GamePacketTypes.CLIENTBOUND_COOLDOWN;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleItemCooldown(this);
    }
}
