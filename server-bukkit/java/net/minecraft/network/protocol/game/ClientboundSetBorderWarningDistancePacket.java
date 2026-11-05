// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.level.border.WorldBorder;
public class ClientboundSetBorderWarningDistancePacket implements Packet<ClientGamePacketListener> {
    private final int warningBlocks;
    public static final StreamCodec<FriendlyByteBuf, ClientboundSetBorderWarningDistancePacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundSetBorderWarningDistancePacket", () -> Packet.codec(ClientboundSetBorderWarningDistancePacket::write, ClientboundSetBorderWarningDistancePacket::new));
    public ClientboundSetBorderWarningDistancePacket(WorldBorder border) {
        this.warningBlocks = border.getWarningBlocks();
    }
    private ClientboundSetBorderWarningDistancePacket(FriendlyByteBuf input) {
        this.warningBlocks = input.readVarInt();
    }
    private void write(FriendlyByteBuf output) {
        output.writeVarInt(this.warningBlocks);
    }
    public PacketType<ClientboundSetBorderWarningDistancePacket> type() {
        return GamePacketTypes.CLIENTBOUND_SET_BORDER_WARNING_DISTANCE;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleSetBorderWarningDistance(this);
    }
    public int getWarningBlocks() {
        return warningBlocks;
    }
}
