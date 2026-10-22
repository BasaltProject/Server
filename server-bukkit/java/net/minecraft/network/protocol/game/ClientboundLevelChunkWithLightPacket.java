package net.minecraft.network.protocol.game;

import java.util.BitSet;
import net.minecraft.network.protocol.Packet;
import net.minecraft.world.level.chunk.LevelChunk;
import net.minecraft.world.level.lighting.LevelLightEngine;

/** Carries only where the chunk is: the world already holds the data, and the server builds the real packet. */
public record ClientboundLevelChunkWithLightPacket(int x, int z, ClientboundLevelChunkPacketData chunkData, Object lightData) implements Packet {
    public ClientboundLevelChunkWithLightPacket(LevelChunk levelChunk, LevelLightEngine levelLightEngine, BitSet bitSet, BitSet bitSet2) {
        this(levelChunk.getPos().x(), levelChunk.getPos().z(), null, null);
    }
}
