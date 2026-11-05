// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.ping;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public record ClientboundPongResponsePacket(long time) implements Packet<ClientPongPacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ClientboundPongResponsePacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundPongResponsePacket", () -> StreamCodec.of((output, packet) -> {
            output.writeLong(packet.time());
        }, input -> new ClientboundPongResponsePacket(input.readLong())));
    private ClientboundPongResponsePacket(FriendlyByteBuf input) {
        this(input.readLong());
    }
    public PacketType<ClientboundPongResponsePacket> type() {
        return PingPacketTypes.CLIENTBOUND_PONG_RESPONSE;
    }
    public void handle(ClientPongPacketListener listener) {
        listener.handlePongResponse(this);
    }
}
