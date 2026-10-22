package net.minecraft.network.protocol.game;

import java.util.BitSet;
import net.minecraft.network.protocol.Packet;
import net.minecraft.world.level.chunk.LevelChunk;
import net.minecraft.world.level.lighting.LevelLightEngine;
public record ClientboundLevelChunkWithLightPacket(int x, int z, ClientboundLevelChunkPacketData chunkData, Object lightData) implements Packet {
    public ClientboundLevelChunkWithLightPacket(LevelChunk levelChunk, LevelLightEngine levelLightEngine, BitSet bitSet, BitSet bitSet2) {
        this(0, 0, null, null);
    }
}
