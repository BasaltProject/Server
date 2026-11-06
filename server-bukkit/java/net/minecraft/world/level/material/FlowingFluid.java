// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.material;

import net.minecraft.world.level.LevelReader;
import java.util.Map;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.StateDefinition;
import net.minecraft.world.level.block.state.properties.BooleanProperty;
import net.minecraft.world.level.block.state.properties.IntegerProperty;
import net.minecraft.world.phys.Vec3;
import net.minecraft.world.phys.shapes.VoxelShape;

public class FlowingFluid extends Fluid {
    private boolean isLava() {
        return this == Fluids.LAVA || this == Fluids.FLOWING_LAVA;
    }

    private final String name;
    private final int tickDelay;

    public FlowingFluid(String name, int tickDelay) {
        this.name = name;
        this.tickDelay = tickDelay;
    }

    @Override
    public int getTickDelay(LevelReader level) {
        return tickDelay;
    }

    @Override
    public String toString() {
        return name;
    }
    public static final BooleanProperty FALLING = net.minecraft.world.level.block.state.properties.BlockStateProperties.FALLING;
    public static final IntegerProperty LEVEL = net.minecraft.world.level.block.state.properties.BlockStateProperties.LEVEL_FLOWING;
    protected void createFluidStateDefinition(StateDefinition.Builder<Fluid, FluidState> builder) {
        builder.add(FALLING);
    }
    public Vec3 getFlow(BlockGetter level, BlockPos pos, FluidState fluidState) {
        return fluidState.getFlow(level, pos);
    }
    protected boolean isSolidFace(BlockGetter level, BlockPos pos, Direction direction) {
        return level.getBlockState(pos).isFaceSturdy(level, pos, direction);
    }
    protected void spread(ServerLevel level, BlockPos pos, BlockState state, FluidState fluidState) {
        fluidState.tick(level, pos, state);
    }
    protected FluidState getNewLiquid(ServerLevel level, BlockPos pos, BlockState state) {
        return state.getFluidState();
    }
    public Fluid getFlowing() {
        return isLava() ? Fluids.FLOWING_LAVA : Fluids.FLOWING_WATER;
    }
    public FluidState getFlowing(int amount, boolean falling) {
        return getFlowing().defaultFluidState().isSource() ? new FluidState(getFlowing(), false) : getFlowing().defaultFluidState();
    }
    public Fluid getSource() {
        return isLava() ? Fluids.LAVA : Fluids.WATER;
    }
    public FluidState getSource(boolean falling) {
        return getSource().defaultFluidState();
    }
    protected boolean canConvertToSource(ServerLevel level) {
        return !isLava();
    }
    protected void spreadTo(LevelAccessor level, BlockPos pos, BlockState state, Direction direction, FluidState target) {
        if (!target.isEmpty()) {
            level.setBlock(pos, target.createLegacyBlock(), 3);
        }
    }
    protected void beforeDestroyingBlock(LevelAccessor level, BlockPos pos, BlockState state) {
        
    }
    protected int getSlopeDistance(LevelReader level, BlockPos pos, int pass, Direction from, BlockState state, SpreadContext context) {
        return 1000;
    }
    protected int getSlopeFindDistance(LevelReader level) {
        return isLava() ? 2 : 4;
    }
    protected Map<Direction, FluidState> getSpread(ServerLevel level, BlockPos pos, BlockState state) {
        return new java.util.EnumMap<>(Direction.class);
    }
    protected int getDropOff(LevelReader level) {
        return isLava() ? 2 : 1;
    }
    protected int getSpreadDelay(Level level, BlockPos pos, FluidState oldFluidState, FluidState newFluidState) {
        return isLava() ? 30 : 5;
    }
    public void tick(ServerLevel level, BlockPos pos, BlockState blockState, FluidState fluidState) {
        fluidState.tick(level, pos, blockState);
    }
    protected static int getLegacyLevel(FluidState fluidState) {
        return fluidState.isSource() ? 0 : 8 - Math.min(fluidState.getAmount(), 8);
    }
    public float getHeight(FluidState fluidState, BlockGetter level, BlockPos pos) {
        return fluidState.getHeight(level, pos);
    }
    public float getOwnHeight(FluidState fluidState) {
        return fluidState.getOwnHeight();
    }
    public int getAmount(FluidState fluidState) {
        return fluidState.getAmount();
    }
    public VoxelShape getShape(FluidState state, BlockGetter level, BlockPos pos) {
        return state.getShape(level, pos);
    }
    static record BlockStatePairKey(BlockState first, BlockState second, Direction direction) {
    }
    protected class SpreadContext {
        private final BlockGetter level;
        private final BlockPos origin;
        private SpreadContext(BlockGetter level, BlockPos origin) {
            this.level = level;
            this.origin = origin;
        }
        public BlockState getBlockState(BlockPos pos) {
            return level.getBlockState(pos);
        }
        public boolean isHole(BlockPos pos) {
            return false;
        }
    }
}
