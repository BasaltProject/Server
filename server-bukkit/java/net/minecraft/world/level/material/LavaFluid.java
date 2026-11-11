// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.material;
import net.minecraft.world.item.Items;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.BaseFireBlock;
import net.minecraft.world.level.block.LiquidBlock;
import net.minecraft.core.particles.ParticleTypes;
import net.minecraft.world.level.gamerules.GameRules;
import net.minecraft.world.entity.InsideBlockEffectType;
import net.minecraft.tags.FluidTags;
import net.minecraft.tags.BlockTags;
import net.minecraft.sounds.SoundEvents;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.particles.ParticleOptions;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.InsideBlockEffectApplier;
import net.minecraft.world.item.Item;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.StateDefinition;
public abstract class LavaFluid extends FlowingFluid {
    protected LavaFluid() {
        super("lava", 30);
    }
    private boolean hasFlammableNeighbours(net.minecraft.world.level.LevelReader level, BlockPos pos) {
        for (Direction side : Direction.values()) {
            if (isFlammable(level, pos.relative(side))) {
                return true;
            }
        }
        return false;
    }
    private boolean isFlammable(net.minecraft.world.level.LevelReader level, BlockPos pos) {
        return (!level.isInsideBuildHeight(pos.getY()) || level.hasChunkAt(pos)) && level.getBlockState(pos).ignitedByLava();
    }
    private void fizz(net.minecraft.world.level.LevelAccessor level, BlockPos pos) {
        level.levelEvent(1501, pos, 0);
    }
    private static boolean isFastLava(net.minecraft.world.level.LevelReader level) {
        return level.environmentAttributes().getDimensionValue(net.minecraft.world.attribute.EnvironmentAttributes.FAST_LAVA);
    }
    public static final int LIGHT_EMISSION = 15;
    public static final float MIN_LEVEL_CUTOFF = 0.44444445F;
    public Fluid getFlowing() {
        return Fluids.FLOWING_LAVA;
    }
    public Fluid getSource() {
        return Fluids.LAVA;
    }
    public Item getBucket() {
        return Items.LAVA_BUCKET;
    }
    public void animateTick(Level level, BlockPos pos, FluidState fluidState, RandomSource random) {
    }
    public void randomTick(ServerLevel level, BlockPos pos, FluidState fluidState, RandomSource random) {
        if (!level.canSpreadFireAround(pos)) {
            return;
        }
        int passes = random.nextInt(3);
        if (passes > 0) {
            BlockPos cursor = pos;
            for (int pass = 0; pass < passes; pass++) {
                cursor = cursor.offset(random.nextInt(3) - 1, 1, random.nextInt(3) - 1);
                if (!level.isLoaded(cursor)) {
                    return;
                }
                BlockState there = level.getBlockState(cursor);
                if (there.isAir()) {
                    if (hasFlammableNeighbours(level, cursor)) {
                        level.setBlockAndUpdate(cursor, BaseFireBlock.getState(level, cursor));
                        return;
                    }
                } else if (there.is(BlockTags.BLOCKS_LAVA_FIRE_SPREAD)) {
                    return;
                }
            }
            return;
        }
        for (int attempt = 0; attempt < 3; attempt++) {
            BlockPos side = pos.offset(random.nextInt(3) - 1, 0, random.nextInt(3) - 1);
            if (!level.isLoaded(side)) {
                return;
            }
            if (level.isEmptyBlock(side.above()) && isFlammable(level, side)) {
                level.setBlockAndUpdate(side.above(), BaseFireBlock.getState(level, side));
            }
        }
    }
    protected void entityInside(Level level, BlockPos pos, Entity entity, InsideBlockEffectApplier effectApplier) {
        effectApplier.apply(InsideBlockEffectType.CLEAR_FREEZE);
        effectApplier.apply(InsideBlockEffectType.LAVA_IGNITE);
        effectApplier.runAfter(InsideBlockEffectType.LAVA_IGNITE, Entity::lavaHurt);
    }
    public ParticleOptions getDripParticle() {
        return ParticleTypes.DRIPPING_LAVA;
    }
    protected void beforeDestroyingBlock(LevelAccessor level, BlockPos pos, BlockState state) {
        fizz(level, pos);
    }
    public int getSlopeFindDistance(LevelReader level) {
        return isFastLava(level) ? 4 : 2;
    }
    public BlockState createLegacyBlock(FluidState fluidState) {
        return Blocks.LAVA.defaultBlockState().setValue(LiquidBlock.LEVEL, getLegacyLevel(fluidState));
    }
    public boolean isSame(Fluid other) {
        return other == Fluids.LAVA || other == Fluids.FLOWING_LAVA;
    }
    public int getDropOff(LevelReader level) {
        return isFastLava(level) ? 1 : 2;
    }
    public boolean canBeReplacedWith(FluidState state, BlockGetter level, BlockPos pos, Fluid other, Direction direction) {
        return state.getHeight(level, pos) >= 0.44444445F && other.is(FluidTags.WATER);
    }
    public int getTickDelay(LevelReader level) {
        return isFastLava(level) ? 10 : 30;
    }
    public int getSpreadDelay(Level level, BlockPos pos, FluidState oldFluidState, FluidState newFluidState) {
        int delay = getTickDelay(level);
        boolean steady = !oldFluidState.isEmpty() && !newFluidState.isEmpty() && !oldFluidState.getValue(FALLING)
            && !newFluidState.getValue(FALLING);
        if (steady && newFluidState.getHeight(level, pos) > oldFluidState.getHeight(level, pos) && level.getRandom().nextInt(4) != 0) {
            delay *= 4;
        }
        return delay;
    }
    protected boolean canConvertToSource(ServerLevel level) {
        return level.getGameRules().get(GameRules.LAVA_SOURCE_CONVERSION);
    }
    protected void spreadTo(LevelAccessor level, BlockPos pos, BlockState state, Direction direction, FluidState target) {
        if (direction == Direction.DOWN && is(FluidTags.LAVA) && level.getFluidState(pos).is(FluidTags.WATER)) {
            if (state.getBlock() instanceof LiquidBlock) {
                level.setBlockAndUpdate(pos, Blocks.STONE.defaultBlockState());
            }
            fizz(level, pos);
            return;
        }
        super.spreadTo(level, pos, state, direction, target);
    }
    protected boolean isRandomlyTicking() {
        return true;
    }
    protected float getExplosionResistance() {
        return 100.0F;
    }
    public Optional<SoundEvent> getPickupSound() {
        return Optional.of(SoundEvents.BUCKET_FILL_LAVA);
    }
    public static class Source extends LavaFluid {
        public int getAmount(FluidState fluidState) {
            return 8;
        }
        public boolean isSource(FluidState fluidState) {
            return true;
        }
    }
    public static class Flowing extends LavaFluid {
        protected void createFluidStateDefinition(StateDefinition.Builder<Fluid, FluidState> builder) {
            super.createFluidStateDefinition(builder);
            builder.add(LEVEL);
        }
        public int getAmount(FluidState fluidState) {
            return fluidState.getValue(LEVEL);
        }
        public boolean isSource(FluidState fluidState) {
            return false;

        }
    }
}
