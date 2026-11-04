// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public class ClientboundContainerClosePacket implements Packet<ClientGamePacketListener> {
    private final int containerId;
    public static final StreamCodec<FriendlyByteBuf, ClientboundContainerClosePacket> STREAM_CODEC = Packet.codec(ClientboundContainerClosePacket::write, ClientboundContainerClosePacket::new);
    public ClientboundContainerClosePacket(int containerId) {
        this.containerId = containerId;
    }
    private ClientboundContainerClosePacket(FriendlyByteBuf input) {
        this.containerId = input.readVarInt();
    }
    private void write(FriendlyByteBuf output) {
        output.writeVarInt(this.containerId);
    }
    public PacketType<ClientboundContainerClosePacket> type() {
        return GamePacketTypes.CLIENTBOUND_CONTAINER_CLOSE;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleContainerClose(this);
    }
    public int getContainerId() {
        return containerId;
    }
}
