// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.material;
import net.minecraft.world.item.Items;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.LiquidBlock;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.core.particles.ParticleTypes;
import net.minecraft.world.level.gamerules.GameRules;
import net.minecraft.world.entity.InsideBlockEffectType;
import net.minecraft.tags.FluidTags;
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
public abstract class WaterFluid extends FlowingFluid {
    protected WaterFluid() {
        super("water", 5);
    }
    public Fluid getFlowing() {
        return Fluids.FLOWING_WATER;
    }
    public Fluid getSource() {
        return Fluids.WATER;
    }
    public Item getBucket() {
        return Items.WATER_BUCKET;
    }
    public void animateTick(Level level, BlockPos pos, FluidState fluidState, RandomSource random) {
    }
    public ParticleOptions getDripParticle() {
        return ParticleTypes.DRIPPING_WATER;
    }
    protected boolean canConvertToSource(ServerLevel level) {
        return level.getGameRules().get(GameRules.WATER_SOURCE_CONVERSION);
    }
    protected void beforeDestroyingBlock(LevelAccessor level, BlockPos pos, BlockState state) {
        BlockEntity tile = state.hasBlockEntity() ? level.getBlockEntity(pos) : null;
        Block.dropResources(state, level, pos, tile);
    }
    protected void entityInside(Level level, BlockPos pos, Entity entity, InsideBlockEffectApplier effectApplier) {
        effectApplier.apply(InsideBlockEffectType.EXTINGUISH);
    }
    public int getSlopeFindDistance(LevelReader level) {
        return 4;
    }
    public BlockState createLegacyBlock(FluidState fluidState) {
        return Blocks.WATER.defaultBlockState().setValue(LiquidBlock.LEVEL, getLegacyLevel(fluidState));
    }
    public boolean isSame(Fluid other) {
        return other == Fluids.WATER || other == Fluids.FLOWING_WATER;
    }
    public int getDropOff(LevelReader level) {
        return 1;
    }
    public int getTickDelay(LevelReader level) {
        return 5;
    }
    public boolean canBeReplacedWith(FluidState state, BlockGetter level, BlockPos pos, Fluid other, Direction direction) {
        return direction == Direction.DOWN && !other.is(FluidTags.WATER);
    }
    protected float getExplosionResistance() {
        return 100.0F;
    }
    public Optional<SoundEvent> getPickupSound() {
        return Optional.of(SoundEvents.BUCKET_FILL);
    }
    public static class Source extends WaterFluid {
        public int getAmount(FluidState fluidState) {
            return 8;
        }
        public boolean isSource(FluidState fluidState) {
            return true;
        }
    }
    public static class Flowing extends WaterFluid {
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
