// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import org.jspecify.annotations.Nullable;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.Level;
public class ClientboundSetCameraPacket implements Packet<ClientGamePacketListener> {
    private final int cameraId;
    public static final StreamCodec<FriendlyByteBuf, ClientboundSetCameraPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundSetCameraPacket", () -> Packet.codec(ClientboundSetCameraPacket::write, ClientboundSetCameraPacket::new));
    public ClientboundSetCameraPacket(Entity camera) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundSetCameraPacket#<init>");
    }
    private ClientboundSetCameraPacket(FriendlyByteBuf input) {
        this.cameraId = input.readVarInt();
    }
    private void write(FriendlyByteBuf output) {
        output.writeVarInt(this.cameraId);
    }
    public PacketType<ClientboundSetCameraPacket> type() {
        return GamePacketTypes.CLIENTBOUND_SET_CAMERA;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleSetCamera(this);
    }
    public Entity getEntity(Level level) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundSetCameraPacket#getEntity");
    }
}
