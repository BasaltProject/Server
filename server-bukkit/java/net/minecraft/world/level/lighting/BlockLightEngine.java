// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.lighting;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.chunk.LightChunkGetter;
public final class BlockLightEngine extends LightEngine<BlockLightSectionStorage.BlockDataLayerStorageMap, BlockLightSectionStorage> {
    public BlockLightEngine(LightChunkGetter chunkSource) {
        super(chunkSource, new BlockLightSectionStorage(chunkSource));
    }
    public BlockLightEngine(LightChunkGetter chunkSource, BlockLightSectionStorage storage) {
        super(chunkSource, new BlockLightSectionStorage(chunkSource));
    }
    protected void checkNode(long blockNode) {
        checkBlock(BlockPos.of(blockNode));
    }
    protected void propagateIncrease(long fromNode, long increaseData, int fromLevel) {
    }
    protected void propagateDecrease(long fromNode, long decreaseData) {
    }
    public void propagateLightSources(ChunkPos pos) {
        ChunkPos target = pos;
        checkBlock(new BlockPos(target.getMinBlockX(), 0, target.getMinBlockZ()));
    }
}
