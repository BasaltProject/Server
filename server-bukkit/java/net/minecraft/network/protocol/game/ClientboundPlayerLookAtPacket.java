// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;

import net.minecraft.commands.arguments.EntityAnchorArgument;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.Level;
import net.minecraft.world.phys.Vec3;

public class ClientboundPlayerLookAtPacket implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ClientboundPlayerLookAtPacket> STREAM_CODEC =
        Packet.codec(ClientboundPlayerLookAtPacket::write, ClientboundPlayerLookAtPacket::new);
    private final double x;
    private final double y;
    private final double z;
    private final int entity;
    private final EntityAnchorArgument.Anchor fromAnchor;
    private final EntityAnchorArgument.Anchor toAnchor;
    private final boolean atEntity;

    public ClientboundPlayerLookAtPacket(EntityAnchorArgument.Anchor fromAnchor, double x, double y, double z) {
        this.fromAnchor = fromAnchor;
        this.x = x;
        this.y = y;
        this.z = z;
        this.entity = 0;
        this.atEntity = false;
        this.toAnchor = null;
    }

    public ClientboundPlayerLookAtPacket(EntityAnchorArgument.Anchor fromAnchor, Entity entity, EntityAnchorArgument.Anchor toAnchor) {
        this.fromAnchor = fromAnchor;
        this.entity = entity.getId();
        this.toAnchor = toAnchor;
        Vec3 pos = toAnchor.apply(entity);
        this.x = pos.x;
        this.y = pos.y;
        this.z = pos.z;
        this.atEntity = true;
    }

    private ClientboundPlayerLookAtPacket(FriendlyByteBuf input) {
        this.fromAnchor = input.readEnum(EntityAnchorArgument.Anchor.class);
        this.x = input.readDouble();
        this.y = input.readDouble();
        this.z = input.readDouble();
        this.atEntity = input.readBoolean();
        if (atEntity) {
            this.entity = input.readVarInt();
            this.toAnchor = input.readEnum(EntityAnchorArgument.Anchor.class);
        } else {
            this.entity = 0;
            this.toAnchor = null;
        }
    }

    private void write(FriendlyByteBuf output) {
        output.writeEnum(fromAnchor);
        output.writeDouble(x);
        output.writeDouble(y);
        output.writeDouble(z);
        output.writeBoolean(atEntity);
        if (atEntity) {
            output.writeVarInt(entity);
            output.writeEnum(toAnchor);
        }
    }

    public PacketType<ClientboundPlayerLookAtPacket> type() {
        return GamePacketTypes.CLIENTBOUND_PLAYER_LOOK_AT;
    }

    public void handle(ClientGamePacketListener listener) {
        listener.handleLookAt(this);
    }

    public EntityAnchorArgument.Anchor getFromAnchor() {
        return fromAnchor;
    }

    public Vec3 getPosition(Level level) {
        if (atEntity) {
            Entity target = level.getEntity(entity);
            return target == null ? new Vec3(x, y, z) : toAnchor.apply(target);
        }
        return new Vec3(x, y, z);
    }
}
