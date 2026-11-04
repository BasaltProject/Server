// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.item.ItemStack;
public class ClientboundContainerSetSlotPacket implements Packet<ClientGamePacketListener> {
    private final int containerId;
    private final int stateId;
    private final int slot;
    private final ItemStack itemStack;
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundContainerSetSlotPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundContainerSetSlotPacket", () -> Packet.codec(ClientboundContainerSetSlotPacket::write, ClientboundContainerSetSlotPacket::new));
    public ClientboundContainerSetSlotPacket(int containerId, int stateId, int slot, ItemStack itemStack) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundContainerSetSlotPacket#<init>");
    }
    private ClientboundContainerSetSlotPacket(RegistryFriendlyByteBuf input) {
        this.containerId = input.readContainerId();
        this.stateId = input.readVarInt();
        this.slot = input.readShort();
        this.itemStack = ItemStack.OPTIONAL_STREAM_CODEC.decode(input);
    }
    private void write(RegistryFriendlyByteBuf output) {
        output.writeContainerId(this.containerId);
        output.writeVarInt(this.stateId);
        output.writeShort(this.slot);
        ItemStack.OPTIONAL_STREAM_CODEC.encode(output, this.itemStack);
    }
    public PacketType<ClientboundContainerSetSlotPacket> type() {
        return GamePacketTypes.CLIENTBOUND_CONTAINER_SET_SLOT;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleContainerSetSlot(this);
    }
    public int getContainerId() {
        return containerId;
    }
    public int getSlot() {
        return slot;
    }
    public ItemStack getItem() {
        return itemStack;
    }
    public int getStateId() {
        return stateId;
    }
}
