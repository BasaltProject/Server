// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.damagesource.DamageType;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.Level;
import net.minecraft.world.phys.Vec3;
public record ClientboundDamageEventPacket(int entityId, Holder<DamageType> sourceType, int sourceCauseId, int sourceDirectId, Optional<Vec3> sourcePosition) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundDamageEventPacket> STREAM_CODEC =
        Packet.codec(ClientboundDamageEventPacket::write, ClientboundDamageEventPacket::new);
    public ClientboundDamageEventPacket(Entity entity, DamageSource source) {
        this(entity.getId(), source.typeHolder(), idOrNone(source.getEntity()), idOrNone(source.getDirectEntity()),
            Optional.ofNullable(source.sourcePositionRaw()));
    }
    private ClientboundDamageEventPacket(RegistryFriendlyByteBuf input) {
        this(input.readVarInt(), DamageType.STREAM_CODEC.decode(input), input.readVarInt() - 1, input.readVarInt() - 1,
            input.readOptional(buf -> new Vec3(buf.readDouble(), buf.readDouble(), buf.readDouble())));
    }
    private static int idOrNone(Entity entity) {
        return entity == null ? -1 : entity.getId();
    }
    private void write(RegistryFriendlyByteBuf output) {
        output.writeVarInt(entityId);
        DamageType.STREAM_CODEC.encode(output, sourceType);
        output.writeVarInt(sourceCauseId + 1);
        output.writeVarInt(sourceDirectId + 1);
        output.writeOptional(sourcePosition, (buf, pos) -> {
            buf.writeDouble(pos.x());
            buf.writeDouble(pos.y());
            buf.writeDouble(pos.z());
        });
    }
    public PacketType<ClientboundDamageEventPacket> type() {
        return GamePacketTypes.CLIENTBOUND_DAMAGE_EVENT;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleDamageEvent(this);
    }
    public DamageSource getSource(Level level) {
        Entity direct = level.getEntity(sourceDirectId);
        Entity cause = level.getEntity(sourceCauseId);
        return sourcePosition.isPresent() ? new DamageSource(sourceType, sourcePosition.get())
            : new DamageSource(sourceType, direct, cause);
    }
}
