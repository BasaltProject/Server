// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public class ServerboundContainerClosePacket implements Packet<ServerGamePacketListener> {
    private final int containerId;
    public static final StreamCodec<FriendlyByteBuf, ServerboundContainerClosePacket> STREAM_CODEC = Packet.codec(ServerboundContainerClosePacket::write, ServerboundContainerClosePacket::new);
    public ServerboundContainerClosePacket(int containerId) {
        this.containerId = containerId;
    }
    private ServerboundContainerClosePacket(FriendlyByteBuf input) {
        this.containerId = input.readVarInt();
    }
    private void write(FriendlyByteBuf output) {
        output.writeVarInt(this.containerId);
    }
    public PacketType<ServerboundContainerClosePacket> type() {
        return GamePacketTypes.SERVERBOUND_CONTAINER_CLOSE;
    }
    public void handle(ServerGamePacketListener listener) {
        listener.handleContainerClose(this);
    }
    public int getContainerId() {
        return containerId;
    }
}
