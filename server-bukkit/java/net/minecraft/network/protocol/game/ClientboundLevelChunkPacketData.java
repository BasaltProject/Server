package net.minecraft.network.protocol.game;

import java.util.List;
import net.minecraft.nbt.CompoundTag;

public class ClientboundLevelChunkPacketData {
    private static final int TWO_MEGABYTES = 2097152;
    private final CompoundTag heightmaps;
    private final byte[] buffer;
    private final List<Object> blockEntitiesData;

    protected ClientboundLevelChunkPacketData() {
        this.heightmaps = null;
        this.buffer = new byte[0];
        this.blockEntitiesData = List.of();
    }
}
