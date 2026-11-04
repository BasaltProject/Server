// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public record ClientboundChunkBatchFinishedPacket(int batchSize) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ClientboundChunkBatchFinishedPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundChunkBatchFinishedPacket", () -> StreamCodec.of((output, packet) -> {
            output.writeVarInt(packet.batchSize());
        }, input -> new ClientboundChunkBatchFinishedPacket(input.readVarInt())));
    private ClientboundChunkBatchFinishedPacket(FriendlyByteBuf input) {
        this((int) 0); throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ClientboundChunkBatchFinishedPacket#<init>");
    }
    public PacketType<ClientboundChunkBatchFinishedPacket> type() {
        return GamePacketTypes.CLIENTBOUND_CHUNK_BATCH_FINISHED;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleChunkBatchFinished(this);
    }
}
