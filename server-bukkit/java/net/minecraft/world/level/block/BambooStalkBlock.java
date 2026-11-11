// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.item.context.BlockPlaceContext;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.ScheduledTickAccess;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.StateDefinition;
import net.minecraft.world.level.block.state.properties.BambooLeaves;
import net.minecraft.world.level.block.state.properties.EnumProperty;
import net.minecraft.world.level.block.state.properties.IntegerProperty;
import net.minecraft.world.level.pathfinder.PathComputationType;
import net.minecraft.world.phys.shapes.CollisionContext;
import net.minecraft.world.phys.shapes.VoxelShape;
public class BambooStalkBlock extends Block implements BonemealableBlock {
    public static final IntegerProperty AGE = net.minecraft.world.level.block.state.properties.BlockStateProperties.AGE_1;
    public static final EnumProperty<BambooLeaves> LEAVES = net.minecraft.world.level.block.state.properties.BlockStateProperties.BAMBOO_LEAVES;
    public static final IntegerProperty STAGE = net.minecraft.world.level.block.state.properties.BlockStateProperties.STAGE;
    public static final int MAX_HEIGHT = 16;
    public static final int STAGE_GROWING = 0;
    public static final int STAGE_DONE_GROWING = 1;
    public static final int AGE_THIN_BAMBOO = 0;
    public static final int AGE_THICK_BAMBOO = 1;
    public BambooStalkBlock(BlockBehaviour.Properties properties) {
        super((int) 0);
    }
    protected void createBlockStateDefinition(StateDefinition.Builder<Block, BlockState> builder) {
        super.createBlockStateDefinition(builder);
    }
    protected boolean propagatesSkylightDown(BlockState state) {
        return super.propagatesSkylightDown(state);
    }
    protected VoxelShape getShape(BlockState state, BlockGetter level, BlockPos pos, CollisionContext context) {
        return super.getShape(state, level, pos, context);
    }
    protected boolean isPathfindable(BlockState state, PathComputationType type) {
        return super.isPathfindable(state, type);
    }
    protected VoxelShape getCollisionShape(BlockState state, BlockGetter level, BlockPos pos, CollisionContext context) {
        return super.getCollisionShape(state, level, pos, context);
    }
    protected boolean isCollisionShapeFullBlock(BlockState state, BlockGetter level, BlockPos pos) {
        return super.isCollisionShapeFullBlock(state, level, pos);
    }
    public BlockState getStateForPlacement(BlockPlaceContext context) {
        return super.getStateForPlacement(context);
    }
    protected void tick(BlockState state, ServerLevel level, BlockPos pos, RandomSource random) {
        super.tick(state, level, pos, random);
    }
    protected boolean isRandomlyTicking(BlockState state) {
        return super.isRandomlyTicking(state);
    }
    protected void randomTick(BlockState state, ServerLevel level, BlockPos pos, RandomSource random) {
        super.randomTick(state, level, pos, random);
    }
    protected boolean canSurvive(BlockState state, LevelReader level, BlockPos pos) {
        return super.canSurvive(state, level, pos);
    }
    protected BlockState updateShape(BlockState state, LevelReader level, ScheduledTickAccess ticks, BlockPos pos, Direction directionToNeighbour, BlockPos neighbourPos, BlockState neighbourState, RandomSource random) {
        return super.updateShape(state, level, ticks, pos, directionToNeighbour, neighbourPos, neighbourState, random);
    }
    public boolean isValidBonemealTarget(LevelReader level, BlockPos pos, BlockState state, BonemealSource source) {
        int above = getHeightAboveUpToMax(level, pos);
        int below = getHeightBelowUpToMax(level, pos);
        BlockPos growth = pos.above(above + 1);
        return above + below + 1 < 16 && level.getBlockState(pos.above(above)).getValue(STAGE) != 1
            && level.isInsideBuildHeight(growth) && level.isEmptyBlock(growth);
    }
    public boolean isBonemealSuccess(Level level, RandomSource random, BlockPos pos, BlockState state, BonemealSource source) {
        return true;
    }
    public void performBonemeal(ServerLevel level, RandomSource random, BlockPos pos, BlockState state, BonemealSource source) {
        org.basaltmc.bukkit.WorldEnvNatives.boneMeal(level.getWorld().getName(), pos.getX(), pos.getY(), pos.getZ());
    }
    protected void growBamboo(BlockState state, Level level, BlockPos pos, RandomSource random, int height) {
        BlockState belowState = level.getBlockState(pos.below());
        BlockPos twoBelowPos = pos.below(2);
        BlockState twoBelowState = level.getBlockState(twoBelowPos);
        BambooLeaves leaves = BambooLeaves.NONE;
        if (height >= 1) {
            if (belowState.is(Blocks.BAMBOO) && belowState.getValue(LEAVES) != BambooLeaves.NONE) {
                leaves = BambooLeaves.LARGE;
                if (twoBelowState.is(Blocks.BAMBOO)) {
                    level.setBlockAndUpdate(pos.below(), belowState.setValue(LEAVES, BambooLeaves.SMALL));
                    level.setBlockAndUpdate(twoBelowPos, twoBelowState.setValue(LEAVES, BambooLeaves.NONE));
                }
            } else {
                leaves = BambooLeaves.SMALL;
            }
        }
        int age = state.getValue(AGE) == 1 || twoBelowState.is(Blocks.BAMBOO) ? 1 : 0;
        int stage = (height >= 11 && random.nextFloat() < 0.25F) || height == 15 ? 1 : 0;
        level.setBlockAndUpdate(pos.above(), defaultBlockState().setValue(AGE, age).setValue(LEAVES, leaves).setValue(STAGE, stage));
    }
    protected int getHeightAboveUpToMax(BlockGetter level, BlockPos pos) {
        int height = 0;
        while (height < 16 && level.getBlockState(pos.above(height + 1)).is(Blocks.BAMBOO)) {
            height++;
        }
        return height;
    }
    protected int getHeightBelowUpToMax(BlockGetter level, BlockPos pos) {
        int height = 0;
        while (height < 16 && level.getBlockState(pos.below(height + 1)).is(Blocks.BAMBOO)) {
            height++;
        }
        return height;
    }
}
