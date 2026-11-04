// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import org.jspecify.annotations.Nullable;
import java.util.UUID;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.Entity;
public class ServerboundTeleportToEntityPacket implements Packet<ServerGamePacketListener> {
    private final UUID uuid;
    public static final StreamCodec<FriendlyByteBuf, ServerboundTeleportToEntityPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ServerboundTeleportToEntityPacket", () -> Packet.codec(ServerboundTeleportToEntityPacket::write, ServerboundTeleportToEntityPacket::new));
    public ServerboundTeleportToEntityPacket(UUID uuid) {
        this.uuid = uuid;
    }
    private ServerboundTeleportToEntityPacket(FriendlyByteBuf input) {
        this.uuid = input.readUUID();
    }
    private void write(FriendlyByteBuf output) {
        output.writeUUID(this.uuid);
    }
    public PacketType<ServerboundTeleportToEntityPacket> type() {
        return GamePacketTypes.SERVERBOUND_TELEPORT_TO_ENTITY;
    }
    public void handle(ServerGamePacketListener listener) {
        listener.handleTeleportToEntityPacket(this);
    }
    public Entity getEntity(ServerLevel level) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerboundTeleportToEntityPacket#getEntity");
    }
}
