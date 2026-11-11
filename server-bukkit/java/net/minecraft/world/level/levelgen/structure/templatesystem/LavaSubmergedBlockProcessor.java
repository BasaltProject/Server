// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.Block;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.LevelReader;
public class LavaSubmergedBlockProcessor implements StructureProcessor {
    public static final MapCodec<LavaSubmergedBlockProcessor> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.templatesystem.LavaSubmergedBlockProcessor.class, "MAP_CODEC");
    public static final LavaSubmergedBlockProcessor INSTANCE = new LavaSubmergedBlockProcessor();
    public StructureTemplate. StructureBlockInfo processBlock(LevelReader level, BlockPos targetPosition, BlockPos referencePos, BlockPos templateRelativePos, StructureTemplate.StructureBlockInfo processedBlockInfo, StructurePlaceSettings settings) {
        BlockPos pos = processedBlockInfo.pos();
        boolean wasLava = level.getBlockState(pos).is(Blocks.LAVA);
        return wasLava && !Block.isShapeFullBlock(processedBlockInfo.state().getShape(level, pos))
            ? new StructureTemplate.StructureBlockInfo(pos, Blocks.LAVA.defaultBlockState(), processedBlockInfo.nbt()) : processedBlockInfo;
    }
    public MapCodec<LavaSubmergedBlockProcessor> codec() {
        return MAP_CODEC;
    }
}
