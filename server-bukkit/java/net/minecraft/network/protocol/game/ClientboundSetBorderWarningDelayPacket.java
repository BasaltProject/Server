// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.level.border.WorldBorder;
public class ClientboundSetBorderWarningDelayPacket implements Packet<ClientGamePacketListener> {
    private final int warningDelay;
    public static final StreamCodec<FriendlyByteBuf, ClientboundSetBorderWarningDelayPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundSetBorderWarningDelayPacket", () -> Packet.codec(ClientboundSetBorderWarningDelayPacket::write, ClientboundSetBorderWarningDelayPacket::new));
    public ClientboundSetBorderWarningDelayPacket(WorldBorder border) {
        this.warningDelay = border.getWarningTime();
    }
    private ClientboundSetBorderWarningDelayPacket(FriendlyByteBuf input) {
        this.warningDelay = input.readVarInt();
    }
    private void write(FriendlyByteBuf output) {
        output.writeVarInt(this.warningDelay);
    }
    public PacketType<ClientboundSetBorderWarningDelayPacket> type() {
        return GamePacketTypes.CLIENTBOUND_SET_BORDER_WARNING_DELAY;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleSetBorderWarningDelay(this);
    }
    public int getWarningDelay() {
        return warningDelay;
    }
}
