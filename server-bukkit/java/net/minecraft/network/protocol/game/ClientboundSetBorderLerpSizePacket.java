// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.level.border.WorldBorder;
public class ClientboundSetBorderLerpSizePacket implements Packet<ClientGamePacketListener> {
    private final double oldSize;
    private final double newSize;
    private final long lerpTime;
    public static final StreamCodec<FriendlyByteBuf, ClientboundSetBorderLerpSizePacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundSetBorderLerpSizePacket", () -> Packet.codec(ClientboundSetBorderLerpSizePacket::write, ClientboundSetBorderLerpSizePacket::new));
    public ClientboundSetBorderLerpSizePacket(WorldBorder border) {
        this.oldSize = border.getSize();
        this.newSize = border.getLerpTarget();
        this.lerpTime = border.getLerpTime();
    }
    private ClientboundSetBorderLerpSizePacket(FriendlyByteBuf input) {
        this.oldSize = input.readDouble();
        this.newSize = input.readDouble();
        this.lerpTime = input.readVarLong();
    }
    private void write(FriendlyByteBuf output) {
        output.writeDouble(this.oldSize);
        output.writeDouble(this.newSize);
        output.writeVarLong(this.lerpTime);
    }
    public PacketType<ClientboundSetBorderLerpSizePacket> type() {
        return GamePacketTypes.CLIENTBOUND_SET_BORDER_LERP_SIZE;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleSetBorderLerpSize(this);
    }
    public double getOldSize() {
        return oldSize;
    }
    public double getNewSize() {
        return newSize;
    }
    public long getLerpTime() {
        return lerpTime;
    }
}
