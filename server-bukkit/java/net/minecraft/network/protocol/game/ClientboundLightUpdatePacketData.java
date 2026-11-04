// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;

import io.netty.buffer.ByteBuf;
import java.util.ArrayList;
import java.util.BitSet;
import java.util.List;
import net.minecraft.core.SectionPos;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.LightLayer;
import net.minecraft.world.level.chunk.DataLayer;
import net.minecraft.world.level.lighting.LevelLightEngine;

public record ClientboundLightUpdatePacketData(BitSet skyYMask, BitSet blockYMask, BitSet emptySkyYMask, BitSet emptyBlockYMask, List<byte[]> skyUpdates, List<byte[]> blockUpdates) {
    private static final StreamCodec<ByteBuf, byte[]> LAYER = ByteBufCodecs.byteArray(2048);
    public static final StreamCodec<ByteBuf, ClientboundLightUpdatePacketData> STREAM_CODEC = StreamCodec.composite(
        ByteBufCodecs.BIT_SET, ClientboundLightUpdatePacketData::skyYMask,
        ByteBufCodecs.BIT_SET, ClientboundLightUpdatePacketData::blockYMask,
        ByteBufCodecs.BIT_SET, ClientboundLightUpdatePacketData::emptySkyYMask,
        ByteBufCodecs.BIT_SET, ClientboundLightUpdatePacketData::emptyBlockYMask,
        LAYER.apply(ByteBufCodecs.list()), ClientboundLightUpdatePacketData::skyUpdates,
        LAYER.apply(ByteBufCodecs.list()), ClientboundLightUpdatePacketData::blockUpdates,
        ClientboundLightUpdatePacketData::new);

    public ClientboundLightUpdatePacketData(ChunkPos chunkPos, LevelLightEngine lightEngine, BitSet skyChangedLightSectionFilter, BitSet blockChangedLightSectionFilter) {
        this(new BitSet(), new BitSet(), new BitSet(), new BitSet(), new ArrayList<>(), new ArrayList<>());
        for (int index = 0; index < lightEngine.getLightSectionCount(); index++) {
            SectionPos at = SectionPos.of(chunkPos, lightEngine.getMinLightSection() + index);
            if (skyChangedLightSectionFilter == null || skyChangedLightSectionFilter.get(index)) {
                collect(lightEngine, LightLayer.SKY, at, index, skyYMask, emptySkyYMask, skyUpdates);
            }
            if (blockChangedLightSectionFilter == null || blockChangedLightSectionFilter.get(index)) {
                collect(lightEngine, LightLayer.BLOCK, at, index, blockYMask, emptyBlockYMask, blockUpdates);
            }
        }
    }

    private static void collect(LevelLightEngine engine, LightLayer layer, SectionPos at, int index, BitSet mask, BitSet empty, List<byte[]> updates) {
        DataLayer data = engine.getLayerListener(layer).getDataLayerData(at);
        if (data == null) {
            return;
        }
        if (data.isEmpty()) {
            empty.set(index);
        } else {
            mask.set(index);
            updates.add(data.getData().clone());
        }
    }
}
