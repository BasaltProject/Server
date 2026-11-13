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
import net.minecraft.server.level.ThreadedLevelLightEngine;
import net.minecraft.world.level.lighting.LevelLightEngine;

public final class ClientboundLightUpdatePacketData {
    private static final StreamCodec<ByteBuf, byte[]> LAYER = ByteBufCodecs.byteArray(2048);
    public static final StreamCodec<ByteBuf, ClientboundLightUpdatePacketData> STREAM_CODEC = StreamCodec.composite(
        ByteBufCodecs.BIT_SET, ClientboundLightUpdatePacketData::skyYMask,
        ByteBufCodecs.BIT_SET, ClientboundLightUpdatePacketData::blockYMask,
        ByteBufCodecs.BIT_SET, ClientboundLightUpdatePacketData::emptySkyYMask,
        ByteBufCodecs.BIT_SET, ClientboundLightUpdatePacketData::emptyBlockYMask,
        LAYER.apply(ByteBufCodecs.list()), ClientboundLightUpdatePacketData::skyUpdates,
        LAYER.apply(ByteBufCodecs.list()), ClientboundLightUpdatePacketData::blockUpdates,
        ClientboundLightUpdatePacketData::new);

    private BitSet skyYMask;
    private BitSet blockYMask;
    private BitSet emptySkyYMask;
    private BitSet emptyBlockYMask;
    private List<byte[]> skyUpdates;
    private List<byte[]> blockUpdates;
    private ChunkPos pending;
    private LevelLightEngine lightEngine;
    private BitSet skyFilter;
    private BitSet blockFilter;

    public ClientboundLightUpdatePacketData(BitSet skyYMask, BitSet blockYMask, BitSet emptySkyYMask, BitSet emptyBlockYMask, List<byte[]> skyUpdates, List<byte[]> blockUpdates) {
        this.skyYMask = skyYMask;
        this.blockYMask = blockYMask;
        this.emptySkyYMask = emptySkyYMask;
        this.emptyBlockYMask = emptyBlockYMask;
        this.skyUpdates = skyUpdates;
        this.blockUpdates = blockUpdates;
    }

    /** The light is read when a field is first asked for; the engine's pending writes are flushed now. */
    public ClientboundLightUpdatePacketData(ChunkPos chunkPos, LevelLightEngine lightEngine, BitSet skyChangedLightSectionFilter, BitSet blockChangedLightSectionFilter) {
        if (lightEngine instanceof ThreadedLevelLightEngine threaded) {
            threaded.flush();
        }
        this.pending = chunkPos;
        this.lightEngine = lightEngine;
        this.skyFilter = skyChangedLightSectionFilter;
        this.blockFilter = blockChangedLightSectionFilter;
    }

    private synchronized void materialize() {
        if (pending == null) {
            return;
        }
        ChunkPos chunkPos = pending;
        skyYMask = new BitSet();
        blockYMask = new BitSet();
        emptySkyYMask = new BitSet();
        emptyBlockYMask = new BitSet();
        skyUpdates = new ArrayList<>();
        blockUpdates = new ArrayList<>();
        for (int index = 0; index < lightEngine.getLightSectionCount(); index++) {
            SectionPos at = SectionPos.of(chunkPos, lightEngine.getMinLightSection() + index);
            if (skyFilter == null || skyFilter.get(index)) {
                collect(lightEngine, LightLayer.SKY, at, index, skyYMask, emptySkyYMask, skyUpdates);
            }
            if (blockFilter == null || blockFilter.get(index)) {
                collect(lightEngine, LightLayer.BLOCK, at, index, blockYMask, emptyBlockYMask, blockUpdates);
            }
        }
        pending = null;
        lightEngine = null;
        skyFilter = null;
        blockFilter = null;
    }

    public BitSet skyYMask() {
        materialize();
        return skyYMask;
    }

    public BitSet blockYMask() {
        materialize();
        return blockYMask;
    }

    public BitSet emptySkyYMask() {
        materialize();
        return emptySkyYMask;
    }

    public BitSet emptyBlockYMask() {
        materialize();
        return emptyBlockYMask;
    }

    public List<byte[]> skyUpdates() {
        materialize();
        return skyUpdates;
    }

    public List<byte[]> blockUpdates() {
        materialize();
        return blockUpdates;
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
