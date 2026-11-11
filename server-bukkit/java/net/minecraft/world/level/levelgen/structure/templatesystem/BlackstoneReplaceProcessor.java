// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.templatesystem;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.LevelReader;
public class BlackstoneReplaceProcessor implements StructureProcessor {
    private final java.util.Map<Block, Block> replacements = java.util.Map.ofEntries(
        java.util.Map.entry(Blocks.COBBLESTONE, Blocks.BLACKSTONE), java.util.Map.entry(Blocks.MOSSY_COBBLESTONE, Blocks.BLACKSTONE),
        java.util.Map.entry(Blocks.STONE, Blocks.POLISHED_BLACKSTONE), java.util.Map.entry(Blocks.STONE_BRICKS, Blocks.POLISHED_BLACKSTONE_BRICKS),
        java.util.Map.entry(Blocks.MOSSY_STONE_BRICKS, Blocks.POLISHED_BLACKSTONE_BRICKS),
        java.util.Map.entry(Blocks.COBBLESTONE_STAIRS, Blocks.BLACKSTONE_STAIRS), java.util.Map.entry(Blocks.MOSSY_COBBLESTONE_STAIRS, Blocks.BLACKSTONE_STAIRS),
        java.util.Map.entry(Blocks.STONE_STAIRS, Blocks.POLISHED_BLACKSTONE_STAIRS), java.util.Map.entry(Blocks.STONE_BRICK_STAIRS, Blocks.POLISHED_BLACKSTONE_BRICK_STAIRS),
        java.util.Map.entry(Blocks.MOSSY_STONE_BRICK_STAIRS, Blocks.POLISHED_BLACKSTONE_BRICK_STAIRS),
        java.util.Map.entry(Blocks.COBBLESTONE_SLAB, Blocks.BLACKSTONE_SLAB), java.util.Map.entry(Blocks.MOSSY_COBBLESTONE_SLAB, Blocks.BLACKSTONE_SLAB),
        java.util.Map.entry(Blocks.SMOOTH_STONE_SLAB, Blocks.POLISHED_BLACKSTONE_SLAB), java.util.Map.entry(Blocks.STONE_SLAB, Blocks.POLISHED_BLACKSTONE_SLAB),
        java.util.Map.entry(Blocks.STONE_BRICK_SLAB, Blocks.POLISHED_BLACKSTONE_BRICK_SLAB), java.util.Map.entry(Blocks.MOSSY_STONE_BRICK_SLAB, Blocks.POLISHED_BLACKSTONE_BRICK_SLAB),
        java.util.Map.entry(Blocks.STONE_BRICK_WALL, Blocks.POLISHED_BLACKSTONE_BRICK_WALL), java.util.Map.entry(Blocks.MOSSY_STONE_BRICK_WALL, Blocks.POLISHED_BLACKSTONE_BRICK_WALL),
        java.util.Map.entry(Blocks.COBBLESTONE_WALL, Blocks.BLACKSTONE_WALL), java.util.Map.entry(Blocks.MOSSY_COBBLESTONE_WALL, Blocks.BLACKSTONE_WALL),
        java.util.Map.entry(Blocks.CHISELED_STONE_BRICKS, Blocks.CHISELED_POLISHED_BLACKSTONE),
        java.util.Map.entry(Blocks.CRACKED_STONE_BRICKS, Blocks.CRACKED_POLISHED_BLACKSTONE_BRICKS),
        java.util.Map.entry(Blocks.IRON_BARS, Blocks.IRON_CHAIN));
    public static final MapCodec<BlackstoneReplaceProcessor> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.templatesystem.BlackstoneReplaceProcessor.class, "MAP_CODEC");
    public static final BlackstoneReplaceProcessor INSTANCE = new BlackstoneReplaceProcessor();
    private BlackstoneReplaceProcessor() {
        
    }
    public StructureTemplate. StructureBlockInfo processBlock(LevelReader level, BlockPos targetPosition, BlockPos referencePos, BlockPos templateRelativePos, StructureTemplate.StructureBlockInfo processedBlockInfo, StructurePlaceSettings settings) {
        Block target = replacements.get(processedBlockInfo.state().getBlock());
        if (target == null) {
            return processedBlockInfo;
        }
        BlockState old = processedBlockInfo.state();
        BlockState made = target.defaultBlockState();
        if (old.hasProperty(net.minecraft.world.level.block.StairBlock.FACING)) {
            made = made.setValue(net.minecraft.world.level.block.StairBlock.FACING, old.getValue(net.minecraft.world.level.block.StairBlock.FACING));
        }
        if (old.hasProperty(net.minecraft.world.level.block.StairBlock.HALF)) {
            made = made.setValue(net.minecraft.world.level.block.StairBlock.HALF, old.getValue(net.minecraft.world.level.block.StairBlock.HALF));
        }
        if (old.hasProperty(net.minecraft.world.level.block.SlabBlock.TYPE)) {
            made = made.setValue(net.minecraft.world.level.block.SlabBlock.TYPE, old.getValue(net.minecraft.world.level.block.SlabBlock.TYPE));
        }
        return new StructureTemplate.StructureBlockInfo(processedBlockInfo.pos(), made, processedBlockInfo.nbt());


    }
    public MapCodec<BlackstoneReplaceProcessor> codec() {
        return MAP_CODEC;
    }
}
