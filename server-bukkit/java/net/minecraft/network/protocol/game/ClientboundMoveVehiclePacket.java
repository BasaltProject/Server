// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;

import io.netty.buffer.ByteBuf;
import net.minecraft.core.PositionAndRotation;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.entity.Entity;

public record ClientboundMoveVehiclePacket(PositionAndRotation movingTo) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<ByteBuf, ClientboundMoveVehiclePacket> STREAM_CODEC =
        PositionAndRotation.STREAM_CODEC.map(ClientboundMoveVehiclePacket::new, ClientboundMoveVehiclePacket::movingTo);

    public static ClientboundMoveVehiclePacket fromEntity(Entity entity) {
        return new ClientboundMoveVehiclePacket(PositionAndRotation.of(entity.position(), entity.getYRot(), entity.getXRot()));
    }

    public PacketType<ClientboundMoveVehiclePacket> type() {
        return GamePacketTypes.CLIENTBOUND_MOVE_VEHICLE;
    }

    public void handle(ClientGamePacketListener listener) {
        listener.handleMoveVehicle(this);
    }
}
