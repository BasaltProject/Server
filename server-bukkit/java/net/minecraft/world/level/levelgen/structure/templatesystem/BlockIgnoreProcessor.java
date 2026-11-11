// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.Block;
public class BlockIgnoreProcessor implements StructureProcessor {
    private final List<Block> toIgnore;
    public static final MapCodec<BlockIgnoreProcessor> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.templatesystem.BlockIgnoreProcessor.class, "MAP_CODEC");
    public static final BlockIgnoreProcessor STRUCTURE_BLOCK = new BlockIgnoreProcessor(List.of(net.minecraft.world.level.block.Blocks.STRUCTURE_BLOCK));
    public static final BlockIgnoreProcessor AIR = new BlockIgnoreProcessor(List.of(net.minecraft.world.level.block.Blocks.AIR));
    public static final BlockIgnoreProcessor STRUCTURE_AND_AIR = new BlockIgnoreProcessor(List.of(net.minecraft.world.level.block.Blocks.AIR, net.minecraft.world.level.block.Blocks.STRUCTURE_BLOCK));
    public BlockIgnoreProcessor(List<Block> toIgnore) {
        this.toIgnore = List.copyOf(toIgnore);
    }
    public StructureTemplate. StructureBlockInfo processBlock(LevelReader level, BlockPos targetPosition, BlockPos referencePos, BlockPos templateRelativePos, StructureTemplate.StructureBlockInfo processedBlockInfo, StructurePlaceSettings settings) {
        return toIgnore.contains(processedBlockInfo.state().getBlock()) ? null : processedBlockInfo;
    }
    public MapCodec<BlockIgnoreProcessor> codec() {
        return MAP_CODEC;
    }
}
