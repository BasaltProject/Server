// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.TickRateManager;
public record ClientboundTickingStatePacket(float tickRate, boolean isFrozen) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ClientboundTickingStatePacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundTickingStatePacket", () -> StreamCodec.of((output, packet) -> {
            output.writeFloat(packet.tickRate());
            output.writeBoolean(packet.isFrozen());
        }, input -> new ClientboundTickingStatePacket(input.readFloat(), input.readBoolean())));
    private ClientboundTickingStatePacket(FriendlyByteBuf input) {
        this(input.readFloat(), input.readBoolean());
    }
    public static ClientboundTickingStatePacket from(TickRateManager manager) {
        return new ClientboundTickingStatePacket(manager.tickrate(), manager.isFrozen());
    }
    public PacketType<ClientboundTickingStatePacket> type() {
        return GamePacketTypes.CLIENTBOUND_TICKING_STATE;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleTickingState(this);
    }
}
