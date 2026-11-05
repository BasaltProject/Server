// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.TickRateManager;
public record ClientboundTickingStepPacket(int tickSteps) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ClientboundTickingStepPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundTickingStepPacket", () -> StreamCodec.of((output, packet) -> {
            output.writeVarInt(packet.tickSteps());
        }, input -> new ClientboundTickingStepPacket(input.readVarInt())));
    private ClientboundTickingStepPacket(FriendlyByteBuf input) {
        this(input.readVarInt());
    }
    public static ClientboundTickingStepPacket from(TickRateManager manager) {
        return new ClientboundTickingStepPacket(manager.frozenTicksToRun());
    }
    public PacketType<ClientboundTickingStepPacket> type() {
        return GamePacketTypes.CLIENTBOUND_TICKING_STEP;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleTickingStep(this);
    }
}
