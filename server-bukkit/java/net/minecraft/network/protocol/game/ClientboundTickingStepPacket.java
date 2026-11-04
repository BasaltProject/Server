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
        this((int) 0); throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundTickingStepPacket#<init>");
    }
    public static ClientboundTickingStepPacket from(TickRateManager manager) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundTickingStepPacket#from");
    }
    public PacketType<ClientboundTickingStepPacket> type() {
        return GamePacketTypes.CLIENTBOUND_TICKING_STEP;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleTickingStep(this);
    }
}
