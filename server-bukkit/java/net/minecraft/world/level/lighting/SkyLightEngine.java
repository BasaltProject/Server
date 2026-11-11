// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.lighting;
import net.minecraft.core.BlockPos;
import net.minecraft.core.SectionPos;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.chunk.LightChunkGetter;
public final class SkyLightEngine extends LightEngine<SkyLightSectionStorage.SkyDataLayerStorageMap, SkyLightSectionStorage> {
    public SkyLightEngine(LightChunkGetter chunkSource) {
        super(chunkSource, new SkyLightSectionStorage(chunkSource));
    }
    SkyLightEngine(LightChunkGetter chunkSource, SkyLightSectionStorage storage) {
        super(chunkSource, new SkyLightSectionStorage(chunkSource));
    }
    protected void checkNode(long blockNode) {
        checkBlock(BlockPos.of(blockNode));
    }
    protected void propagateIncrease(long fromNode, long increaseData, int fromLevel) {
    }
    protected void propagateDecrease(long fromNode, long decreaseData) {
    }
    public void setLightEnabled(ChunkPos pos, boolean enable) {
        storage.setLightEnabled(SectionPos.getZeroNode(SectionPos.asLong(pos.x(), 0, pos.z())), enable);
    }
    public void propagateLightSources(ChunkPos pos) {
        checkBlock(new BlockPos(pos.getMinBlockX(), 0, pos.getMinBlockZ()));


    }
}
