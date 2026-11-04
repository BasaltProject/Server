// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;

import java.util.BitSet;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.level.chunk.LevelChunk;
import net.minecraft.world.level.lighting.LevelLightEngine;

public record ClientboundLevelChunkWithLightPacket(int x, int z, ClientboundLevelChunkPacketData chunkData, ClientboundLightUpdatePacketData lightData) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundLevelChunkWithLightPacket> STREAM_CODEC = StreamCodec.composite(
        ByteBufCodecs.INT, ClientboundLevelChunkWithLightPacket::x,
        ByteBufCodecs.INT, ClientboundLevelChunkWithLightPacket::z,
        ClientboundLevelChunkPacketData.STREAM_CODEC, ClientboundLevelChunkWithLightPacket::chunkData,
        ClientboundLightUpdatePacketData.STREAM_CODEC, ClientboundLevelChunkWithLightPacket::lightData,
        ClientboundLevelChunkWithLightPacket::new);

    public ClientboundLevelChunkWithLightPacket(LevelChunk levelChunk, LevelLightEngine lightEngine, BitSet skyChangedLightSectionFilter, BitSet blockChangedLightSectionFilter) {
        this(levelChunk.getPos().x(), levelChunk.getPos().z(), new ClientboundLevelChunkPacketData(levelChunk),
            new ClientboundLightUpdatePacketData(levelChunk.getPos(), lightEngine, skyChangedLightSectionFilter, blockChangedLightSectionFilter));
    }

    public PacketType<ClientboundLevelChunkWithLightPacket> type() {
        return GamePacketTypes.CLIENTBOUND_LEVEL_CHUNK_WITH_LIGHT;
    }

    public void handle(ClientGamePacketListener listener) {
        listener.handleLevelChunkWithLight(this);
    }
}
