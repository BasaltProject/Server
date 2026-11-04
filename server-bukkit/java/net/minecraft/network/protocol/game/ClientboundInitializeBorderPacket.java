// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.level.border.WorldBorder;
public class ClientboundInitializeBorderPacket implements Packet<ClientGamePacketListener> {
    private final double newCenterX;
    private final double newCenterZ;
    private final double oldSize;
    private final double newSize;
    private final long lerpTime;
    private final int newAbsoluteMaxSize;
    private final int warningBlocks;
    private final int warningTime;
    public static final StreamCodec<FriendlyByteBuf, ClientboundInitializeBorderPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundInitializeBorderPacket", () -> Packet.codec(ClientboundInitializeBorderPacket::write, ClientboundInitializeBorderPacket::new));
    private ClientboundInitializeBorderPacket(FriendlyByteBuf input) {
        this.newCenterX = input.readDouble();
        this.newCenterZ = input.readDouble();
        this.oldSize = input.readDouble();
        this.newSize = input.readDouble();
        this.lerpTime = input.readVarLong();
        this.newAbsoluteMaxSize = input.readVarInt();
        this.warningBlocks = input.readVarInt();
        this.warningTime = input.readVarInt();
    }
    public ClientboundInitializeBorderPacket(WorldBorder border) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundInitializeBorderPacket#<init>");
    }
    private void write(FriendlyByteBuf output) {
        output.writeDouble(this.newCenterX);
        output.writeDouble(this.newCenterZ);
        output.writeDouble(this.oldSize);
        output.writeDouble(this.newSize);
        output.writeVarLong(this.lerpTime);
        output.writeVarInt(this.newAbsoluteMaxSize);
        output.writeVarInt(this.warningBlocks);
        output.writeVarInt(this.warningTime);
    }
    public PacketType<ClientboundInitializeBorderPacket> type() {
        return GamePacketTypes.CLIENTBOUND_INITIALIZE_BORDER;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleInitializeBorder(this);
    }
    public double getNewCenterX() {
        return newCenterX;
    }
    public double getNewCenterZ() {
        return newCenterZ;
    }
    public double getNewSize() {
        return newSize;
    }
    public double getOldSize() {
        return oldSize;
    }
    public long getLerpTime() {
        return lerpTime;
    }
    public int getNewAbsoluteMaxSize() {
        return newAbsoluteMaxSize;
    }
    public int getWarningTime() {
        return warningTime;
    }
    public int getWarningBlocks() {
        return warningBlocks;
    }
}
