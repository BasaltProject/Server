// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public class ClientboundMountScreenOpenPacket implements Packet<ClientGamePacketListener> {
    private final int containerId;
    private final int inventoryColumns;
    private final int entityId;
    public static final StreamCodec<FriendlyByteBuf, ClientboundMountScreenOpenPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundMountScreenOpenPacket", () -> Packet.codec(ClientboundMountScreenOpenPacket::write, ClientboundMountScreenOpenPacket::new));
    public ClientboundMountScreenOpenPacket(int containerId, int inventoryColumns, int entityId) {
        this.containerId = containerId;
        this.inventoryColumns = inventoryColumns;
        this.entityId = entityId;
    }
    private ClientboundMountScreenOpenPacket(FriendlyByteBuf input) {
        this.containerId = input.readContainerId();
        this.inventoryColumns = input.readVarInt();
        this.entityId = input.readInt();
    }
    private void write(FriendlyByteBuf output) {
        output.writeContainerId(this.containerId);
        output.writeVarInt(this.inventoryColumns);
        output.writeInt(this.entityId);
    }
    public PacketType<ClientboundMountScreenOpenPacket> type() {
        return GamePacketTypes.CLIENTBOUND_MOUNT_SCREEN_OPEN;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleMountScreenOpen(this);
    }
    public int getContainerId() {
        return containerId;
    }
    public int getInventoryColumns() {
        return inventoryColumns;
    }
    public int getEntityId() {
        return entityId;
    }
}
