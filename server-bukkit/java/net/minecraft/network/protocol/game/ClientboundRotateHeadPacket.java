// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.util.Mth;
import org.jspecify.annotations.Nullable;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.Level;
public class ClientboundRotateHeadPacket implements Packet<ClientGamePacketListener> {
    private final int entityId;
    private final byte yHeadRot;
    public static final StreamCodec<FriendlyByteBuf, ClientboundRotateHeadPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundRotateHeadPacket", () -> Packet.codec(ClientboundRotateHeadPacket::write, ClientboundRotateHeadPacket::new));
    public ClientboundRotateHeadPacket(Entity entity, byte yHeadRot) {
        this.entityId = entity.getId();
        this.yHeadRot = yHeadRot;
    }
    private ClientboundRotateHeadPacket(FriendlyByteBuf input) {
        this.entityId = input.readVarInt();
        this.yHeadRot = input.readByte();
    }
    private void write(FriendlyByteBuf output) {
        output.writeVarInt(this.entityId);
        output.writeByte(this.yHeadRot);
    }
    public PacketType<ClientboundRotateHeadPacket> type() {
        return GamePacketTypes.CLIENTBOUND_ROTATE_HEAD;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleRotateMob(this);
    }
    public Entity getEntity(Level level) {
        return level.getEntity(entityId);
    }
    public float getYHeadRot() {
        return Mth.unpackDegrees(yHeadRot);
    }
}
