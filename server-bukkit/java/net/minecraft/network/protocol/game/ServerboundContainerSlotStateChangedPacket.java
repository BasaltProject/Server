// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public record ServerboundContainerSlotStateChangedPacket(int slotId, int containerId, boolean newState) implements Packet<ServerGamePacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ServerboundContainerSlotStateChangedPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ServerboundContainerSlotStateChangedPacket", () -> StreamCodec.of((output, packet) -> {
            output.writeVarInt(packet.slotId());
            output.writeContainerId(packet.containerId());
            output.writeBoolean(packet.newState());
        }, input -> new ServerboundContainerSlotStateChangedPacket(input.readVarInt(), input.readContainerId(), input.readBoolean())));
    private ServerboundContainerSlotStateChangedPacket(FriendlyByteBuf input) {
        this(input.readVarInt(), input.readContainerId(), input.readBoolean());
    }
    public PacketType<ServerboundContainerSlotStateChangedPacket> type() {
        return GamePacketTypes.SERVERBOUND_CONTAINER_SLOT_STATE_CHANGED;
    }
    public void handle(ServerGamePacketListener listener) {
        listener.handleContainerSlotStateChanged(this);
    }
}
