// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;

import com.mojang.datafixers.util.Pair;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.item.ItemStack;

public class ClientboundSetEquipmentPacket implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundSetEquipmentPacket> STREAM_CODEC =
        Packet.codec(ClientboundSetEquipmentPacket::write, ClientboundSetEquipmentPacket::new);
    private static final int kContinueFlag = 0x80;
    private final int entity;
    private final List<Pair<EquipmentSlot, ItemStack>> slots;

    public ClientboundSetEquipmentPacket(int entity, List<Pair<EquipmentSlot, ItemStack>> slots) {
        this.entity = entity;
        this.slots = slots;
    }

    private ClientboundSetEquipmentPacket(RegistryFriendlyByteBuf input) {
        this.entity = input.readVarInt();
        this.slots = new ArrayList<>();
        int header;
        do {
            header = input.readByte();
            EquipmentSlot slot = EquipmentSlot.VALUES.get(header & ~kContinueFlag & 0x7f);
            slots.add(Pair.of(slot, ItemStack.OPTIONAL_STREAM_CODEC.decode(input)));
        } while ((header & kContinueFlag) != 0);
    }

    private void write(RegistryFriendlyByteBuf output) {
        output.writeVarInt(entity);
        for (int i = 0; i < slots.size(); i++) {
            Pair<EquipmentSlot, ItemStack> entry = slots.get(i);
            int id = entry.getFirst().ordinal();
            output.writeByte(i != slots.size() - 1 ? id | kContinueFlag : id);
            ItemStack.OPTIONAL_STREAM_CODEC.encode(output, entry.getSecond());
        }
    }

    public PacketType<ClientboundSetEquipmentPacket> type() {
        return GamePacketTypes.CLIENTBOUND_SET_EQUIPMENT;
    }

    public void handle(ClientGamePacketListener listener) {
        listener.handleSetEquipment(this);
    }

    public int getEntity() {
        return entity;
    }

    public List<Pair<EquipmentSlot, ItemStack>> getSlots() {
        return slots;
    }
}
