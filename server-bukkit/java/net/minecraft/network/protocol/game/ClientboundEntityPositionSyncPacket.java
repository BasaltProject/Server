// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.PositionPath;
public record ClientboundEntityPositionSyncPacket(int id, PositionPath position, float yRot, float xRot, boolean onGround) implements MovementPacket<ClientGamePacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ClientboundEntityPositionSyncPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundEntityPositionSyncPacket.STREAM_CODEC", () -> StreamCodec.composite(ByteBufCodecs.VAR_INT, ClientboundEntityPositionSyncPacket::id, PositionPath.STREAM_CODEC, ClientboundEntityPositionSyncPacket::position, ByteBufCodecs.FLOAT, ClientboundEntityPositionSyncPacket::yRot, ByteBufCodecs.FLOAT, ClientboundEntityPositionSyncPacket::xRot, ByteBufCodecs.BOOL, ClientboundEntityPositionSyncPacket::onGround, ClientboundEntityPositionSyncPacket::new));
    public static ClientboundEntityPositionSyncPacket of(Entity entity, PositionPath position) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundEntityPositionSyncPacket#of");
    }
    public static ClientboundEntityPositionSyncPacket of(Entity entity) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundEntityPositionSyncPacket#of");
    }
    public PacketType<ClientboundEntityPositionSyncPacket> type() {
        return GamePacketTypes.CLIENTBOUND_ENTITY_POSITION_SYNC;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleEntityPositionSync(this);
    }
    public boolean hasPosition() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundEntityPositionSyncPacket#hasPosition");
    }
    public boolean hasRotation() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundEntityPositionSyncPacket#hasRotation");
    }
}
