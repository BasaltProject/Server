// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.InsideBlockEffectApplier;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.ScheduledTickAccess;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.StateDefinition;
import net.minecraft.world.level.block.state.properties.BooleanProperty;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.phys.shapes.CollisionContext;
import net.minecraft.world.phys.shapes.VoxelShape;
public class BubbleColumnBlock extends Block implements BucketPickup {
    private static boolean canOccupy(Block bubbleColumn, BlockState occupyState) {
        if (occupyState.is(bubbleColumn)) {
            return true;
        }
        FluidState fluid = occupyState.getFluidState();
        return fluid.is(net.minecraft.tags.FluidTags.BUBBLE_COLUMN_CAN_OCCUPY) && occupyState.getBlock() instanceof LiquidBlock
            && fluid.isSource() && fluid.getAmount() >= 8;
    }
    private static BlockState columnState(Block bubbleColumn, BlockState belowState, BlockState occupyState) {
        if (belowState.is(bubbleColumn)) {
            return belowState;
        }
        if (belowState.is(net.minecraft.tags.BlockTags.ENABLES_BUBBLE_COLUMN_PUSH_UP)) {
            return bubbleColumn.defaultBlockState().setValue(DRAG_DOWN, false);
        }
        if (belowState.is(net.minecraft.tags.BlockTags.ENABLES_BUBBLE_COLUMN_DRAG_DOWN)) {
            return bubbleColumn.defaultBlockState().setValue(DRAG_DOWN, true);
        }
        return occupyState.is(bubbleColumn) ? Blocks.WATER.defaultBlockState() : occupyState;
    }
    public static final BooleanProperty DRAG_DOWN = net.minecraft.world.level.block.state.properties.BlockStateProperties.DRAG;
    public BubbleColumnBlock(BlockBehaviour.Properties properties) {
        super((int) 0);
    }
    protected void entityInside(BlockState state, Level level, BlockPos pos, Entity entity, InsideBlockEffectApplier effectApplier, boolean isPrecise) {
        super.entityInside(state, level, pos, entity, effectApplier, isPrecise);
    }
    protected void tick(BlockState state, ServerLevel level, BlockPos pos, RandomSource random) {
        super.tick(state, level, pos, random);
    }
    protected FluidState getFluidState(BlockState state) {
        return super.getFluidState(state);
    }
    public static void updateColumn(Block bubbleColumn, LevelAccessor level, BlockPos occupyAt, BlockState belowState) {
        updateColumn(bubbleColumn, level, occupyAt, level.getBlockState(occupyAt), belowState);
    }
    public static void updateColumn(Block bubbleColumn, LevelAccessor level, BlockPos occupyAt, BlockState occupyState, BlockState belowState) {
        if (!canOccupy(bubbleColumn, occupyState)) {
            return;
        }
        BlockState column = columnState(bubbleColumn, belowState, occupyState);
        level.setBlock(occupyAt, column, 2);
        BlockPos.MutableBlockPos cursor = occupyAt.mutable().move(Direction.UP);
        while (canOccupy(bubbleColumn, level.getBlockState(cursor))) {
            if (!level.setBlock(cursor, column, 2)) {
                return;
            }
            cursor.move(Direction.UP);
        }
    }
    public void animateTick(BlockState state, Level level, BlockPos pos, RandomSource random) {
        super.animateTick(state, level, pos, random);
    }
    protected BlockState updateShape(BlockState state, LevelReader level, ScheduledTickAccess ticks, BlockPos pos, Direction directionToNeighbour, BlockPos neighbourPos, BlockState neighbourState, RandomSource random) {
        return super.updateShape(state, level, ticks, pos, directionToNeighbour, neighbourPos, neighbourState, random);
    }
    protected boolean canSurvive(BlockState state, LevelReader level, BlockPos pos) {
        return super.canSurvive(state, level, pos);
    }
    protected VoxelShape getShape(BlockState state, BlockGetter level, BlockPos pos, CollisionContext context) {
        return super.getShape(state, level, pos, context);
    }
    protected RenderShape getRenderShape(BlockState state) {
        return super.getRenderShape(state);
    }
    protected void createBlockStateDefinition(StateDefinition.Builder<Block, BlockState> builder) {
        super.createBlockStateDefinition(builder);
    }
    public ItemStack pickupBlock(LivingEntity user, LevelAccessor level, BlockPos pos, BlockState state) {
        level.setBlock(pos, Blocks.AIR.defaultBlockState(), 11);
        return new ItemStack(net.minecraft.world.item.Items.WATER_BUCKET);
    }
    public Optional<SoundEvent> getPickupSound() {
        return net.minecraft.world.level.material.Fluids.WATER.getPickupSound();
    }
}
