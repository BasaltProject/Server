// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.common;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public record ClientboundTransferPacket(String host, int port) implements Packet<ClientCommonPacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ClientboundTransferPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundTransferPacket", () -> StreamCodec.of((output, packet) -> {
            output.writeUtf(packet.host());
            output.writeVarInt(packet.port());
        }, input -> new ClientboundTransferPacket(input.readUtf(), input.readVarInt())));
    private ClientboundTransferPacket(FriendlyByteBuf input) {
        this(input.readUtf(), input.readVarInt());
    }
    public PacketType<ClientboundTransferPacket> type() {
        return CommonPacketTypes.CLIENTBOUND_TRANSFER;
    }
    public void handle(ClientCommonPacketListener listener) {
        listener.handleTransfer(this);
    }
}
