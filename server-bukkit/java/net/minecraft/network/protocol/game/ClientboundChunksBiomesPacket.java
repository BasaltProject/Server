// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import io.netty.buffer.Unpooled;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.world.level.chunk.LevelChunkSection;
import io.netty.buffer.ByteBuf;
import java.util.List;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.chunk.LevelChunk;
public record ClientboundChunksBiomesPacket(List<ChunkBiomeData> chunkBiomeData) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<ByteBuf, ClientboundChunksBiomesPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundChunksBiomesPacket.STREAM_CODEC", () -> StreamCodec.composite(ClientboundChunksBiomesPacket.ChunkBiomeData.STREAM_CODEC.apply(ByteBufCodecs.list()), ClientboundChunksBiomesPacket::chunkBiomeData, ClientboundChunksBiomesPacket::new));
    public static ClientboundChunksBiomesPacket forChunks(List<LevelChunk> chunks) {
        return new ClientboundChunksBiomesPacket(chunks.stream().map(ChunkBiomeData::new).toList());
    }
    public PacketType<ClientboundChunksBiomesPacket> type() {
        return GamePacketTypes.CLIENTBOUND_CHUNKS_BIOMES;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleChunksBiomes(this);
    }
    public static record ChunkBiomeData(ChunkPos pos, byte[] buffer) {
        public static final StreamCodec<ByteBuf, ChunkBiomeData> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundChunksBiomesPacket.ChunkBiomeData.STREAM_CODEC", () -> StreamCodec.composite(ChunkPos.STREAM_CODEC, ChunkBiomeData::pos, ByteBufCodecs.byteArray(2097152), ChunkBiomeData::buffer, ChunkBiomeData::new));
        public ChunkBiomeData(LevelChunk chunk) {
            this(chunk.getPos(), new byte[biomeBytes(chunk)]);
            ByteBuf sink = Unpooled.wrappedBuffer(buffer);
            sink.writerIndex(0);
            extractChunkData(new FriendlyByteBuf(sink), chunk);
        }
        private static int biomeBytes(LevelChunk chunk) {
            int total = 0;
            for (LevelChunkSection section : chunk.getSections()) {
                total += section.getBiomes().getSerializedSize();
            }
            return total;
        }
        public FriendlyByteBuf getReadBuffer() {
            return new FriendlyByteBuf(Unpooled.wrappedBuffer(buffer));
        }
        public static void extractChunkData(FriendlyByteBuf buffer, LevelChunk chunk) {
            for (LevelChunkSection section : chunk.getSections()) {
                section.getBiomes().write(buffer);
            }
        }
    }
}
