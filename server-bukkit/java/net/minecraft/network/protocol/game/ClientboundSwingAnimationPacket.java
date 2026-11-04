// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.codec.ByteBufCodecs;
import io.netty.buffer.ByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.item.component.SwingAnimation;
public record ClientboundSwingAnimationPacket(int entityId, InteractionHand hand, SwingAnimation animation) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<ByteBuf, ClientboundSwingAnimationPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundSwingAnimationPacket.STREAM_CODEC", () -> StreamCodec.composite(ByteBufCodecs.VAR_INT, ClientboundSwingAnimationPacket::entityId, InteractionHand.STREAM_CODEC, ClientboundSwingAnimationPacket::hand, SwingAnimation.STREAM_CODEC, ClientboundSwingAnimationPacket::animation, ClientboundSwingAnimationPacket::new));
    public ClientboundSwingAnimationPacket(Entity entity, InteractionHand hand, SwingAnimation animation) {
        this((int) 0, (InteractionHand) null, (SwingAnimation) null); throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundSwingAnimationPacket#<init>");
    }
    public PacketType<ClientboundSwingAnimationPacket> type() {
        return GamePacketTypes.CLIENTBOUND_SWING_ANIMATION;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleSwingAnimation(this);
    }
}
