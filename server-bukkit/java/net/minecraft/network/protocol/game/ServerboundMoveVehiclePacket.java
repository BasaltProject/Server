// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.core.PositionAndRotation;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.entity.Entity;
public record ServerboundMoveVehiclePacket(PositionAndRotation movingTo, boolean onGround) implements Packet<ServerGamePacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ServerboundMoveVehiclePacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ServerboundMoveVehiclePacket.STREAM_CODEC", () -> StreamCodec.composite(PositionAndRotation.STREAM_CODEC, ServerboundMoveVehiclePacket::movingTo, ByteBufCodecs.BOOL, ServerboundMoveVehiclePacket::onGround, ServerboundMoveVehiclePacket::new));
    public static ServerboundMoveVehiclePacket fromEntity(Entity entity) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerboundMoveVehiclePacket#fromEntity");
    }
    public PacketType<ServerboundMoveVehiclePacket> type() {
        return GamePacketTypes.SERVERBOUND_MOVE_VEHICLE;
    }
    public void handle(ServerGamePacketListener listener) {
        listener.handleMoveVehicle(this);
    }
}
