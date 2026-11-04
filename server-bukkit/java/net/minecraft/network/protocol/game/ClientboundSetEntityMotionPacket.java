// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.codec.ByteBufCodecs;
import io.netty.buffer.ByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.phys.Vec3;
public record ClientboundSetEntityMotionPacket(int id, Vec3 movement) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<ByteBuf, ClientboundSetEntityMotionPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundSetEntityMotionPacket.STREAM_CODEC", () -> StreamCodec.composite(ByteBufCodecs.VAR_INT, ClientboundSetEntityMotionPacket::id, Vec3.LP_STREAM_CODEC, ClientboundSetEntityMotionPacket::movement, ClientboundSetEntityMotionPacket::new));
    public ClientboundSetEntityMotionPacket(Entity entity) {
        this((int) 0, (Vec3) null); throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundSetEntityMotionPacket#<init>");
    }
    public PacketType<ClientboundSetEntityMotionPacket> type() {
        return GamePacketTypes.CLIENTBOUND_SET_ENTITY_MOTION;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleSetEntityMotion(this);
    }
}
