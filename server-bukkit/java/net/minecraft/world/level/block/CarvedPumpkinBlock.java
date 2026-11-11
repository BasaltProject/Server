// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.world.level.block.state.pattern.BlockInWorld;
import net.minecraft.world.level.block.state.pattern.BlockPatternBuilder;
import net.minecraft.world.level.block.state.predicate.BlockStatePredicate;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.item.context.BlockPlaceContext;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.StateDefinition;
import net.minecraft.world.level.block.state.pattern.BlockPattern;
import net.minecraft.world.level.block.state.properties.EnumProperty;
public class CarvedPumpkinBlock extends HorizontalDirectionalBlock {
    private BlockPattern snowBase;
    private BlockPattern ironBase;
    private BlockPattern copperBase;
    private BlockPattern snowGolemBase() {
        if (snowBase == null) {
            snowBase = BlockPatternBuilder.start().aisle(" ", "#", "#")
                .where('#', BlockInWorld.hasState(BlockStatePredicate.forBlock(Blocks.SNOW_BLOCK))).build();
        }
        return snowBase;
    }
    private BlockPattern ironGolemBase() {
        if (ironBase == null) {
            ironBase = BlockPatternBuilder.start().aisle("~ ~", "###", "~#~")
                .where('#', BlockInWorld.hasState(BlockStatePredicate.forBlock(Blocks.IRON_BLOCK)))
                .where('~', BlockInWorld.hasState(BlockBehaviour.BlockStateBase::isAir)).build();
        }
        return ironBase;
    }
    private BlockPattern copperGolemBase() {
        if (copperBase == null) {
            copperBase = BlockPatternBuilder.start().aisle(" ", "#")
                .where('#', BlockInWorld.hasState(block -> block.is(net.minecraft.tags.BlockTags.COPPER))).build();
        }
        return copperBase;
    }
    public static final EnumProperty<Direction> FACING = net.minecraft.world.level.block.state.properties.BlockStateProperties.HORIZONTAL_FACING;
    protected CarvedPumpkinBlock(BlockBehaviour.Properties properties) {
        super((int) 0);
    }
    protected void onPlace(BlockState state, Level level, BlockPos pos, BlockState oldState, boolean movedByPiston) {
        super.onPlace(state, level, pos, oldState, movedByPiston);
    }
    public boolean canSpawnGolem(LevelReader level, BlockPos topPos) {
        return snowGolemBase().find(level, topPos) != null || ironGolemBase().find(level, topPos) != null
            || copperGolemBase().find(level, topPos) != null;
    }
    public static void clearPatternBlocks(Level level, BlockPattern.BlockPatternMatch match) {
        for (int x = 0; x < match.getWidth(); x++) {
            for (int y = 0; y < match.getHeight(); y++) {
                BlockInWorld block = match.getBlock(x, y, 0);
                level.setBlock(block.getPos(), Blocks.AIR.defaultBlockState(), 2);
                level.levelEvent(2001, block.getPos(), Block.getId(block.getState()));
            }
        }
    }
    public static void updatePatternBlocks(Level level, BlockPattern.BlockPatternMatch match) {
        for (int x = 0; x < match.getWidth(); x++) {
            for (int y = 0; y < match.getHeight(); y++) {
                level.updateNeighborsAt(match.getBlock(x, y, 0).getPos(), Blocks.AIR);
            }
        }
    }
    public BlockState getStateForPlacement(BlockPlaceContext context) {
        return super.getStateForPlacement(context);
    }
    protected void createBlockStateDefinition(StateDefinition.Builder<Block, BlockState> builder) {
        super.createBlockStateDefinition(builder);
    }
    public void replaceCopperBlockWithChest(Level level, BlockPattern.BlockPatternMatch match) {
        BlockInWorld copper = match.getBlock(0, 1, 0);
        Direction facing = match.getBlock(0, 0, 0).getState().getValue(FACING);
        level.setBlock(copper.getPos(), CopperChestBlock.getFromCopperBlock(copper.getState().getBlock(), facing, level, copper.getPos()), 2);
    }
}
