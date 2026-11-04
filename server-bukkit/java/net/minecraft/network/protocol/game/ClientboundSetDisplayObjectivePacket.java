// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;

import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.scores.DisplaySlot;
import net.minecraft.world.scores.Objective;

public class ClientboundSetDisplayObjectivePacket implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ClientboundSetDisplayObjectivePacket> STREAM_CODEC =
        Packet.codec(ClientboundSetDisplayObjectivePacket::write, ClientboundSetDisplayObjectivePacket::new);
    private final DisplaySlot slot;
    private final String objectiveName;

    public ClientboundSetDisplayObjectivePacket(DisplaySlot slot, Objective objective) {
        this.slot = slot;
        this.objectiveName = objective == null ? "" : objective.getName();
    }

    private ClientboundSetDisplayObjectivePacket(FriendlyByteBuf input) {
        this.slot = input.readById(DisplaySlot.BY_ID);
        this.objectiveName = input.readUtf();
    }

    private void write(FriendlyByteBuf output) {
        output.writeById(DisplaySlot::id, slot);
        output.writeUtf(objectiveName);
    }

    public PacketType<ClientboundSetDisplayObjectivePacket> type() {
        return GamePacketTypes.CLIENTBOUND_SET_DISPLAY_OBJECTIVE;
    }

    public void handle(ClientGamePacketListener listener) {
        listener.handleSetDisplayObjective(this);
    }

    public DisplaySlot getSlot() {
        return slot;
    }

    public String getObjectiveName() {
        return objectiveName.isEmpty() ? null : objectiveName;
    }
}
