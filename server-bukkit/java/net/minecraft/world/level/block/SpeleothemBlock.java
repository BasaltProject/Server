// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.tags.BlockTags;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.world.level.material.Fluids;
import java.util.Optional;
import java.util.function.BiPredicate;
import java.util.function.Predicate;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.item.FallingBlockEntity;
import net.minecraft.world.entity.projectile.Projectile;
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
import net.minecraft.world.level.block.state.properties.EnumProperty;
import net.minecraft.world.level.block.state.properties.SpeleothemThickness;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.level.pathfinder.PathComputationType;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.shapes.CollisionContext;
import net.minecraft.world.phys.shapes.VoxelShape;
public abstract class SpeleothemBlock extends Block implements SimpleWaterloggedBlock, Fallable {
    private static boolean sameFacing(BlockState state, Direction tip) {
        return state.is(BlockTags.SPELEOTHEMS) && state.getValue(TIP_DIRECTION) == tip;
    }
    private static boolean isTip(BlockState state, boolean includeMerged) {
        if (!state.is(BlockTags.SPELEOTHEMS)) {
            return false;
        }
        SpeleothemThickness kind = state.getValue(THICKNESS);
        return kind == SpeleothemThickness.TIP || includeMerged && kind == SpeleothemThickness.TIP_MERGE;
    }
    private boolean validPlacement(LevelReader level, BlockPos pos, Direction tip) {
        BlockPos behind = pos.relative(tip.getOpposite());
        BlockState support = level.getBlockState(behind);
        return support.isFaceSturdy(level, behind, tip) || sameFacing(support, tip) && support.is(this);
    }
    private boolean isUnmergedTip(BlockState state, Direction tip) {
        return isTip(state, false) && state.getValue(TIP_DIRECTION) == tip && state.is(this);
    }
    private boolean tipCanGrow(BlockState tipState, ServerLevel level, BlockPos tipPos) {
        Direction toward = tipState.getValue(TIP_DIRECTION);
        BlockState ahead = level.getBlockState(tipPos.relative(toward));
        if (!ahead.getFluidState().isEmpty()) {
            return false;
        }
        return ahead.isAir() || isUnmergedTip(ahead, toward.getOpposite());
    }
    private void place(LevelAccessor level, BlockPos pos, Direction tip, SpeleothemThickness kind) {
        level.setBlockAndUpdate(pos, defaultBlockState().setValue(TIP_DIRECTION, tip).setValue(THICKNESS, kind)
            .setValue(WATERLOGGED, level.getFluidState(pos).is(Fluids.WATER)));
    }
    private void growInto(ServerLevel level, BlockPos from, Direction toward) {
        BlockPos target = from.relative(toward);
        BlockState there = level.getBlockState(target);
        if (isUnmergedTip(there, toward.getOpposite())) {
            boolean up = there.getValue(TIP_DIRECTION) == Direction.UP;
            place(level, up ? target.above() : target, Direction.DOWN, SpeleothemThickness.TIP_MERGE);
            place(level, up ? target : target.below(), Direction.UP, SpeleothemThickness.TIP_MERGE);
        } else if (there.isAir() || there.is(Blocks.WATER)) {
            place(level, target, toward, SpeleothemThickness.TIP);
        }
    }
    private void growStalagmiteBelow(ServerLevel level, BlockPos above) {
        BlockPos.MutableBlockPos cursor = above.mutable();
        for (int step = 0; step < 10; step++) {
            cursor.move(Direction.DOWN);
            BlockState here = level.getBlockState(cursor);
            if (!here.getFluidState().isEmpty()) {
                return;
            }
            if (isUnmergedTip(here, Direction.UP) && tipCanGrow(here, level, cursor)) {
                growInto(level, cursor, Direction.UP);
                return;
            }
            if (validPlacement(level, cursor, Direction.UP) && !level.isWaterAt(cursor.below())) {
                growInto(level, cursor.below(), Direction.UP);
                return;
            }
            if (blocksStalagmiteScan(level, cursor, here)) {
                return;
            }
        }
    }
    public static final EnumProperty<Direction> TIP_DIRECTION = net.minecraft.world.level.block.state.properties.BlockStateProperties.VERTICAL_DIRECTION;
    public static final EnumProperty<SpeleothemThickness> THICKNESS = net.minecraft.world.level.block.state.properties.BlockStateProperties.SPELEOTHEM_THICKNESS;
    public static final BooleanProperty WATERLOGGED = net.minecraft.world.level.block.state.properties.BlockStateProperties.WATERLOGGED;
    protected static final VoxelShape SHAPE_TIP_DOWN = Block.column(6.0, 5.0, 16.0);
    protected final BlockState blockToGrowOn = null;
    public SpeleothemBlock(BlockState blockToGrowOn, BlockBehaviour.Properties properties) {
        super((int) 0);
    }
    protected void createBlockStateDefinition(StateDefinition.Builder<Block, BlockState> builder) {
        super.createBlockStateDefinition(builder);
    }
    protected boolean canSurvive(BlockState state, LevelReader level, BlockPos pos) {
        return super.canSurvive(state, level, pos);
    }
    protected BlockState updateShape(BlockState state, LevelReader level, ScheduledTickAccess ticks, BlockPos pos, Direction directionToNeighbour, BlockPos neighbourPos, BlockState neighbourState, RandomSource random) {
        return super.updateShape(state, level, ticks, pos, directionToNeighbour, neighbourPos, neighbourState, random);
    }
    public BlockState getStateForPlacement(BlockPlaceContext context) {
        return super.getStateForPlacement(context);
    }
    protected void onProjectileHit(Level level, BlockState state, BlockHitResult blockHit, Projectile projectile) {
        super.onProjectileHit(level, state, blockHit, projectile);
    }
    protected void tick(BlockState state, ServerLevel level, BlockPos pos, RandomSource random) {
        super.tick(state, level, pos, random);
    }
    protected static boolean isStalactite(BlockState state) {
        return sameFacing(state, Direction.DOWN);
    }
    public void onBrokenAfterFall(Level level, BlockPos pos, FallingBlockEntity entity) {
        if (!entity.isSilent()) {
            level.levelEvent(getStalactiteLandingSound(), pos, 0);
        }
    }
    protected abstract int getStalactiteLandingSound();
    public DamageSource getFallDamageSource(Entity entity) {
        return entity.damageSources().fallingStalactite(entity);
    }
    protected FluidState getFluidState(BlockState state) {
        return super.getFluidState(state);
    }
    protected VoxelShape getShape(BlockState state, BlockGetter level, BlockPos pos, CollisionContext context) {
        return super.getShape(state, level, pos, context);
    }
    protected boolean isCollisionShapeFullBlock(BlockState state, BlockGetter level, BlockPos pos) {
        return super.isCollisionShapeFullBlock(state, level, pos);
    }
    protected float getMaxHorizontalOffset() {
        return super.getMaxHorizontalOffset();
    }
    protected boolean isPathfindable(BlockState state, PathComputationType type) {
        return super.isPathfindable(state, type);
    }
    protected void randomTick(BlockState state, ServerLevel level, BlockPos pos, RandomSource random) {
        super.randomTick(state, level, pos, random);
    }
    protected static boolean isStalactiteStartPos(BlockState state, LevelReader level, BlockPos pos) {
        return isStalactite(state) && !level.getBlockState(pos.above()).is(state.getBlock());
    }
    public void growStalactiteOrStalagmiteIfPossible(BlockState stalactiteStartState, ServerLevel level, BlockPos stalactiteStartPos, RandomSource random) {
        if (!canGrow(level, stalactiteStartPos)) {
            return;
        }
        BlockPos tipPos = findTip(stalactiteStartState, level, stalactiteStartPos, getMaxGrowthLength(), false);
        if (tipPos == null) {
            return;
        }
        BlockState tip = level.getBlockState(tipPos);
        if (isFreeHangingStalactite(tip) && tipCanGrow(tip, level, tipPos)) {
            if (random.nextBoolean()) {
                growInto(level, tipPos, Direction.DOWN);
            } else {
                growStalagmiteBelow(level, tipPos);
            }
        }
    }
    protected boolean canGrow(LevelReader level, BlockPos pos) {
        return level.getBlockState(pos.above()).is(blockToGrowOn.getBlock());
    }
    protected static BlockPos findTip(BlockState speleothemState, LevelAccessor level, BlockPos speleothemPos, int maxSearchLength, boolean includeMergedTip) {
        if (isTip(speleothemState, includeMergedTip)) {
            return speleothemPos;
        }
        Direction along = speleothemState.getValue(TIP_DIRECTION);
        BiPredicate<BlockPos, BlockState> onPath = (p, s) -> s.is(speleothemState.getBlock()) && s.getValue(TIP_DIRECTION) == along;
        return findBlockVertical(level, speleothemPos, along.getAxisDirection(), onPath, s -> isTip(s, includeMergedTip), maxSearchLength).orElse(null);
    }
    protected static Optional<BlockPos> findBlockVertical(LevelAccessor level, BlockPos pos, Direction.AxisDirection axisDirection, BiPredicate<BlockPos, BlockState> pathPredicate, Predicate<BlockState> targetPredicate, int maxSteps) {
        Direction step = Direction.get(axisDirection, Direction.Axis.Y);
        BlockPos.MutableBlockPos cursor = pos.mutable();
        for (int i = 1; i < maxSteps; i++) {
            cursor.move(step);
            BlockState here = level.getBlockState(cursor);
            if (targetPredicate.test(here)) {
                return Optional.of(cursor.immutable());
            }
            if (level.isOutsideBuildHeight(cursor.getY()) || !pathPredicate.test(cursor, here)) {
                break;
            }
        }
        return Optional.empty();
    }
    protected boolean blocksStalagmiteScan(LevelReader level, BlockPos pos, BlockState state) {
        return false;
    }
    protected static boolean isFreeHangingStalactite(BlockState state) {
        return isStalactite(state) && state.getValue(THICKNESS) == SpeleothemThickness.TIP && !state.getValue(WATERLOGGED);
    }
    protected int getMaxGrowthLength() {
        return 7;


    }
}
