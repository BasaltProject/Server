// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.core.BlockPos;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public class ClientboundLevelEventPacket implements Packet<ClientGamePacketListener> {
    private final int type;
    private final BlockPos pos;
    private final int data;
    private final boolean globalEvent;
    public static final StreamCodec<FriendlyByteBuf, ClientboundLevelEventPacket> STREAM_CODEC = Packet.codec(ClientboundLevelEventPacket::write, ClientboundLevelEventPacket::new);
    public ClientboundLevelEventPacket(int type, BlockPos pos, int data, boolean globalEvent) {
        this.type = type;
        this.pos = pos;
        this.data = data;
        this.globalEvent = globalEvent;
    }
    private ClientboundLevelEventPacket(FriendlyByteBuf input) {
        this.type = input.readInt();
        this.pos = input.readBlockPos();
        this.data = input.readInt();
        this.globalEvent = input.readBoolean();
    }
    private void write(FriendlyByteBuf output) {
        output.writeInt(this.type);
        output.writeBlockPos(this.pos);
        output.writeInt(this.data);
        output.writeBoolean(this.globalEvent);
    }
    public PacketType<ClientboundLevelEventPacket> type() {
        return GamePacketTypes.CLIENTBOUND_LEVEL_EVENT;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleLevelEvent(this);
    }
    public boolean isGlobalEvent() {
        return globalEvent;
    }
    public int getType() {
        return type;
    }
    public int getData() {
        return data;
    }
    public BlockPos getPos() {
        return pos;
    }
}
