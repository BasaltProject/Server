// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import java.util.function.ToIntFunction;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.util.RandomSource;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.context.BlockPlaceContext;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.ScheduledTickAccess;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.StateDefinition;
import net.minecraft.world.level.block.state.properties.BooleanProperty;
import net.minecraft.world.level.block.state.properties.IntegerProperty;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.Vec3;
import net.minecraft.world.phys.shapes.CollisionContext;
import net.minecraft.world.phys.shapes.VoxelShape;
public class CandleBlock extends AbstractCandleBlock implements SimpleWaterloggedBlock {
    private static final java.util.Map<Integer, java.util.List<Vec3>> kParticleOffsets = java.util.Map.of(
        1, java.util.List.of(new Vec3(0.5, 0.5, 0.5)),
        2, java.util.List.of(new Vec3(0.375, 0.4375, 0.5), new Vec3(0.625, 0.5, 0.4375)),
        3, java.util.List.of(new Vec3(0.5, 0.3125, 0.625), new Vec3(0.375, 0.4375, 0.5), new Vec3(0.5625, 0.5, 0.4375)),
        4, java.util.List.of(new Vec3(0.4375, 0.3125, 0.5625), new Vec3(0.625, 0.4375, 0.5625),
            new Vec3(0.375, 0.4375, 0.375), new Vec3(0.5625, 0.5, 0.375)));
    public static final int MIN_CANDLES = 1;
    public static final int MAX_CANDLES = 4;
    public static final IntegerProperty CANDLES = net.minecraft.world.level.block.state.properties.BlockStateProperties.CANDLES;
    public static final BooleanProperty LIT = net.minecraft.world.level.block.state.properties.BlockStateProperties.LIT;
    public static final BooleanProperty WATERLOGGED = net.minecraft.world.level.block.state.properties.BlockStateProperties.WATERLOGGED;
    public static final ToIntFunction<BlockState> LIGHT_EMISSION = state -> state.getValue(LIT) ? 3 * state.getValue(CANDLES) : 0;
    public CandleBlock(BlockBehaviour.Properties properties) {
        super((net.minecraft.world.level.block.state.BlockBehaviour.Properties) null);
    }
    protected InteractionResult useItemOn(ItemStack itemStack, BlockState state, Level level, BlockPos pos, Player player, InteractionHand hand, BlockHitResult hitResult) {
        return super.useItemOn(itemStack, state, level, pos, player, hand, hitResult);
    }
    protected boolean canBeReplaced(BlockState state, BlockPlaceContext context) {
        return super.canBeReplaced(state, context);
    }
    public BlockState getStateForPlacement(BlockPlaceContext context) {
        return super.getStateForPlacement(context);
    }
    protected BlockState updateShape(BlockState state, LevelReader level, ScheduledTickAccess ticks, BlockPos pos, Direction directionToNeighbour, BlockPos neighbourPos, BlockState neighbourState, RandomSource random) {
        return super.updateShape(state, level, ticks, pos, directionToNeighbour, neighbourPos, neighbourState, random);
    }
    protected FluidState getFluidState(BlockState state) {
        return super.getFluidState(state);
    }
    protected VoxelShape getShape(BlockState state, BlockGetter level, BlockPos pos, CollisionContext context) {
        return super.getShape(state, level, pos, context);
    }
    protected void createBlockStateDefinition(StateDefinition.Builder<Block, BlockState> builder) {
        super.createBlockStateDefinition(builder);
    }
    public boolean placeLiquid(LevelAccessor level, BlockPos pos, BlockState state, FluidState fluidState) {
        if (state.getValue(WATERLOGGED) || !fluidState.is(net.minecraft.world.level.material.Fluids.WATER)) {
            return false;
        }
        BlockState newState = state.setValue(WATERLOGGED, true);
        if (state.getValue(LIT)) {
            extinguish(null, newState, level, pos);
        } else {
            level.setBlock(pos, newState, 3);
        }
        level.scheduleTick(pos, fluidState.getType(), fluidState.getType().getTickDelay(level));
        return true;
    }
    public static boolean canLight(BlockState state) {
        return state.is(net.minecraft.tags.BlockTags.CANDLES, s -> s.hasProperty(LIT) && s.hasProperty(WATERLOGGED))
            && !state.getValue(LIT) && !state.getValue(WATERLOGGED);
    }
    protected Iterable<Vec3> getParticleOffsets(BlockState state) {
        return kParticleOffsets.get(state.getValue(CANDLES));
    }
    protected boolean canBeLit(BlockState state) {
        return !state.getValue(WATERLOGGED) && super.canBeLit(state);
    }
    protected boolean canSurvive(BlockState state, LevelReader level, BlockPos pos) {
        return super.canSurvive(state, level, pos);
    }
}
