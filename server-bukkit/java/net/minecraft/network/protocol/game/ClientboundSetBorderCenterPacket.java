// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.level.border.WorldBorder;
public class ClientboundSetBorderCenterPacket implements Packet<ClientGamePacketListener> {
    private final double newCenterX;
    private final double newCenterZ;
    public static final StreamCodec<FriendlyByteBuf, ClientboundSetBorderCenterPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundSetBorderCenterPacket", () -> Packet.codec(ClientboundSetBorderCenterPacket::write, ClientboundSetBorderCenterPacket::new));
    public ClientboundSetBorderCenterPacket(WorldBorder border) {
        this.newCenterX = border.getCenterX();
        this.newCenterZ = border.getCenterZ();
    }
    private ClientboundSetBorderCenterPacket(FriendlyByteBuf input) {
        this.newCenterX = input.readDouble();
        this.newCenterZ = input.readDouble();
    }
    private void write(FriendlyByteBuf output) {
        output.writeDouble(this.newCenterX);
        output.writeDouble(this.newCenterZ);
    }
    public PacketType<ClientboundSetBorderCenterPacket> type() {
        return GamePacketTypes.CLIENTBOUND_SET_BORDER_CENTER;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleSetBorderCenter(this);
    }
    public double getNewCenterZ() {
        return newCenterZ;
    }
    public double getNewCenterX() {
        return newCenterX;
    }
}
