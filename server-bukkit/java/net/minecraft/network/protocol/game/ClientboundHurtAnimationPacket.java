// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.entity.LivingEntity;
public record ClientboundHurtAnimationPacket(int id, float yaw) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ClientboundHurtAnimationPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundHurtAnimationPacket", () -> StreamCodec.of((output, packet) -> {
            output.writeVarInt(packet.id());
            output.writeFloat(packet.yaw());
        }, input -> new ClientboundHurtAnimationPacket(input.readVarInt(), input.readFloat())));
    public ClientboundHurtAnimationPacket(LivingEntity entity) {
        this(entity.getId(), entity.getHurtDir());
    }
    private ClientboundHurtAnimationPacket(FriendlyByteBuf input) {
        this(input.readVarInt(), input.readFloat());
    }
    public PacketType<ClientboundHurtAnimationPacket> type() {
        return GamePacketTypes.CLIENTBOUND_HURT_ANIMATION;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleHurtAnimation(this);
    }
}
