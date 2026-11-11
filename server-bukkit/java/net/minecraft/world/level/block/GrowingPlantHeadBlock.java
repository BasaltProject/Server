// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.ScheduledTickAccess;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.StateDefinition;
import net.minecraft.world.level.block.state.properties.IntegerProperty;
import net.minecraft.world.phys.shapes.VoxelShape;
public abstract class GrowingPlantHeadBlock extends GrowingPlantBlock implements BonemealableBlock {
    private double growPerTickProbability;
    public static final IntegerProperty AGE = net.minecraft.world.level.block.state.properties.BlockStateProperties.AGE_25;
    public static final int MAX_AGE = 25;
    protected GrowingPlantHeadBlock(BlockBehaviour.Properties properties, Direction growthDirection, VoxelShape shape, boolean scheduleFluidTicks, double growPerTickProbability) {
        super(properties, growthDirection, shape, scheduleFluidTicks);
        this.growPerTickProbability = growPerTickProbability;
    }
    public BlockState getStateForPlacement(RandomSource random) {
        return defaultBlockState().setValue(AGE, random.nextInt(25));
    }
    protected boolean isRandomlyTicking(BlockState state) {
        return super.isRandomlyTicking(state);
    }
    protected void randomTick(BlockState state, ServerLevel level, BlockPos pos, RandomSource random) {
        super.randomTick(state, level, pos, random);
    }
    protected BlockState getGrowIntoState(BlockState growFromState, RandomSource random) {
        return growFromState.cycle(AGE);
    }
    public BlockState getMaxAgeState(BlockState fromState) {
        return fromState.setValue(AGE, 25);
    }
    public boolean isMaxAge(BlockState state) {
        return state.getValue(AGE) == 25;
    }
    protected BlockState updateBodyAfterConvertedFromHead(BlockState headState, BlockState bodyState) {
        return bodyState;
    }
    protected BlockState updateShape(BlockState state, LevelReader level, ScheduledTickAccess ticks, BlockPos pos, Direction directionToNeighbour, BlockPos neighbourPos, BlockState neighbourState, RandomSource random) {
        return super.updateShape(state, level, ticks, pos, directionToNeighbour, neighbourPos, neighbourState, random);
    }
    protected void createBlockStateDefinition(StateDefinition.Builder<Block, BlockState> builder) {
        super.createBlockStateDefinition(builder);
    }
    public boolean isValidBonemealTarget(LevelReader level, BlockPos pos, BlockState state, BonemealSource source) {
        BlockPos growth = pos.relative(growthDirection);
        return canGrowInto(level.getBlockState(growth)) && level.isInsideBuildHeight(growth);
    }
    public boolean isBonemealSuccess(Level level, RandomSource random, BlockPos pos, BlockState state, BonemealSource source) {
        return true;
    }
    public void performBonemeal(ServerLevel level, RandomSource random, BlockPos pos, BlockState state, BonemealSource source) {
        org.basaltmc.bukkit.WorldEnvNatives.boneMeal(level.getWorld().getName(), pos.getX(), pos.getY(), pos.getZ());
    }
    protected abstract int getBlocksToGrowWhenBonemealed(RandomSource random);
    protected abstract boolean canGrowInto(BlockState state);
    protected GrowingPlantHeadBlock getHeadBlock() {
        return this;
    }
}
