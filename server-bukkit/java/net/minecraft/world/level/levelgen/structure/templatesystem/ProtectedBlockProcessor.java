// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.core.HolderSet;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.Block;
public record ProtectedBlockProcessor(HolderSet<Block> cannotReplace) implements StructureProcessor {
    public static final MapCodec<ProtectedBlockProcessor> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.templatesystem.ProtectedBlockProcessor.class, "MAP_CODEC");
    public StructureTemplate. StructureBlockInfo processBlock(LevelReader level, BlockPos targetPosition, BlockPos referencePos, BlockPos templateRelativePos, StructureTemplate.StructureBlockInfo processedBlockInfo, StructurePlaceSettings settings) {
        return !level.getBlockState(processedBlockInfo.pos()).is(cannotReplace) ? processedBlockInfo : null;
    }
    public MapCodec<ProtectedBlockProcessor> codec() {
        return MAP_CODEC;
    }
}
