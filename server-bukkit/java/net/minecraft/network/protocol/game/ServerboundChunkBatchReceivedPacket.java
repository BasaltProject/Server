// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
public record ServerboundChunkBatchReceivedPacket(float desiredChunksPerTick) implements Packet<ServerGamePacketListener> {
    public static final StreamCodec<FriendlyByteBuf, ServerboundChunkBatchReceivedPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ServerboundChunkBatchReceivedPacket", () -> StreamCodec.of((output, packet) -> {
            output.writeFloat(packet.desiredChunksPerTick());
        }, input -> new ServerboundChunkBatchReceivedPacket(input.readFloat())));
    private ServerboundChunkBatchReceivedPacket(FriendlyByteBuf input) {
        this(input.readFloat());
    }
    public PacketType<ServerboundChunkBatchReceivedPacket> type() {
        return GamePacketTypes.SERVERBOUND_CHUNK_BATCH_RECEIVED;
    }
    public void handle(ServerGamePacketListener listener) {
        listener.handleChunkBatchReceived(this);
    }
}
