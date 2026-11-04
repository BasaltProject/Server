// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public class ClientboundContainerSetDataPacket implements Packet<ClientGamePacketListener> {
    private final int containerId;
    private final int id;
    private final int value;
    public static final StreamCodec<FriendlyByteBuf, ClientboundContainerSetDataPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundContainerSetDataPacket", () -> Packet.codec(ClientboundContainerSetDataPacket::write, ClientboundContainerSetDataPacket::new));
    public ClientboundContainerSetDataPacket(int containerId, int id, int value) {
        this.containerId = containerId;
        this.id = id;
        this.value = value;
    }
    private ClientboundContainerSetDataPacket(FriendlyByteBuf input) {
        this.containerId = input.readContainerId();
        this.id = input.readShort();
        this.value = input.readShort();
    }
    private void write(FriendlyByteBuf output) {
        output.writeContainerId(this.containerId);
        output.writeShort(this.id);
        output.writeShort(this.value);
    }
    public PacketType<ClientboundContainerSetDataPacket> type() {
        return GamePacketTypes.CLIENTBOUND_CONTAINER_SET_DATA;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleContainerSetData(this);
    }
    public int getContainerId() {
        return containerId;
    }
    public int getId() {
        return id;
    }
    public int getValue() {
        return value;
    }
}
