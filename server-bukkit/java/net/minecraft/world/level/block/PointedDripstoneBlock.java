// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import java.util.Optional;
import net.minecraft.core.Direction;
import net.minecraft.world.phys.shapes.VoxelShape;
import net.minecraft.world.level.material.FluidState;
import java.util.function.BiPredicate;
import java.util.function.Predicate;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.level.material.Fluids;
import net.minecraft.world.phys.shapes.BooleanOp;
import net.minecraft.world.phys.shapes.Shapes;
import net.minecraft.world.attribute.EnvironmentAttributes;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.material.Fluid;
public class PointedDripstoneBlock extends SpeleothemBlock {
    private static final VoxelShape kDripSpace = Block.column(4.0, 0.0, 16.0);
    private static boolean fillsCauldron(Fluid fluid) {
        return fluid == Fluids.LAVA || fluid == Fluids.WATER;
    }
    private static boolean dripsThrough(BlockGetter level, BlockPos pos, BlockState state) {
        if (state.isAir()) {
            return true;
        }
        if (state.isSolidRender() || !state.getFluidState().isEmpty()) {
            return false;
        }
        return !Shapes.joinIsNotEmpty(kDripSpace, state.getCollisionShape(level, pos), BooleanOp.AND);
    }
    private static Optional<FluidInfo> fluidAbove(Level level, BlockPos pos, BlockState state) {
        if (!isStalactite(state)) {
            return Optional.empty();
        }
        Direction tip = state.getValue(TIP_DIRECTION);
        BiPredicate<BlockPos, BlockState> onPath = (p, s) -> s.is(state.getBlock()) && s.getValue(TIP_DIRECTION) == tip;
        Optional<BlockPos> root = findBlockVertical(level, pos, tip.getOpposite().getAxisDirection(), onPath, s -> !s.is(state.getBlock()), 11);
        if (root.isEmpty()) {
            return Optional.empty();
        }
        BlockPos abovePos = root.get().above();
        BlockState aboveState = level.getBlockState(abovePos);
        boolean mud = aboveState.is(Blocks.MUD) && !level.environmentAttributes().getValue(EnvironmentAttributes.WATER_EVAPORATES, abovePos);
        return Optional.of(new FluidInfo(abovePos, mud ? Fluids.WATER : level.getFluidState(abovePos).getType(), aboveState));
    }
    private static BlockPos fillableCauldronBelow(Level level, BlockPos tipPos, Fluid fluid) {
        Predicate<BlockState> wanted = s -> s.getBlock() instanceof AbstractCauldronBlock cauldron && cauldron.canReceiveStalactiteDrip(fluid);
        return findBlockVertical(level, tipPos, Direction.AxisDirection.NEGATIVE, (p, s) -> dripsThrough(level, p, s), wanted, 11).orElse(null);
    }
    public PointedDripstoneBlock(BlockState blockToGrowOn, BlockBehaviour.Properties properties) {
        super((net.minecraft.world.level.block.state.BlockState) null, (net.minecraft.world.level.block.state.BlockBehaviour.Properties) null);
    }
    protected int getStalactiteLandingSound() {
        return 1045;
    }
    public void fallOn(Level level, BlockState state, BlockPos pos, Entity entity, double fallDistance) {
        super.fallOn(level, state, pos, entity, fallDistance);
    }
    public void animateTick(BlockState state, Level level, BlockPos pos, RandomSource random) {
        super.animateTick(state, level, pos, random);
    }
    protected void randomTick(BlockState state, ServerLevel level, BlockPos pos, RandomSource random) {
        super.randomTick(state, level, pos, random);
    }
    public static void maybeTransferFluid(BlockState state, ServerLevel level, BlockPos pos, float randomValue) {
        if (randomValue > 0.17578125F && randomValue > 0.05859375F || !isStalactiteStartPos(state, level, pos)) {
            return;
        }
        Optional<FluidInfo> info = fluidAbove(level, pos, state);
        if (info.isEmpty()) {
            return;
        }
        Fluid fluid = info.get().fluid;
        float chance = fluid == Fluids.WATER ? 0.17578125F : fluid == Fluids.LAVA ? 0.05859375F : -1.0F;
        if (chance < 0.0F || randomValue >= chance) {
            return;
        }
        BlockPos tipPos = findTip(state, level, pos, 11, false);
        if (tipPos == null) {
            return;
        }
        if (info.get().sourceState.is(Blocks.MUD) && fluid == Fluids.WATER) {
            BlockState clay = Blocks.CLAY.defaultBlockState();
            level.setBlockAndUpdate(info.get().pos, clay);
            Block.pushEntitiesUp(info.get().sourceState, clay, level, info.get().pos);
            level.gameEvent(GameEvent.BLOCK_CHANGE, info.get().pos, GameEvent.Context.of(clay));
            level.levelEvent(1504, tipPos, 0);
            return;
        }
        BlockPos cauldron = fillableCauldronBelow(level, tipPos, fluid);
        if (cauldron != null) {
            level.levelEvent(1504, tipPos, 0);
            level.scheduleTick(cauldron, level.getBlockState(cauldron).getBlock(), 50 + tipPos.getY() - cauldron.getY());
        }
    }
    public static void spawnDripParticle(Level level, BlockPos stalactiteTipPos, BlockState stalactiteTipState) {
        return;
    }
    public static BlockPos findStalactiteTipAboveCauldron(Level level, BlockPos cauldronPos) {
        return findBlockVertical(level, cauldronPos, Direction.AxisDirection.POSITIVE, (p, s) -> dripsThrough(level, p, s), SpeleothemBlock::isFreeHangingStalactite, 11).orElse(null);
    }
    public static Fluid getCauldronFillFluidType(ServerLevel level, BlockPos stalactitePos) {
        Fluid above = fluidAbove(level, stalactitePos, level.getBlockState(stalactitePos)).map(info -> info.fluid).orElse(Fluids.EMPTY);
        return fillsCauldron(above) ? above : Fluids.EMPTY;
    }
    protected boolean canGrow(LevelReader level, BlockPos pos) {
        FluidState water = level.getBlockState(pos.above(2)).getFluidState();
        return super.canGrow(level, pos) && water.is(Fluids.WATER) && water.isSource();
    }
    protected boolean blocksStalagmiteScan(LevelReader level, BlockPos pos, BlockState state) {
        return !dripsThrough(level, pos, state);


    }
    static record FluidInfo(BlockPos pos, Fluid fluid, BlockState sourceState) {
    }
}
