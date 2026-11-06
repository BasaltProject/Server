// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.material;

import net.minecraft.world.level.LevelReader;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.Holder;
import net.minecraft.core.IdMapper;
import net.minecraft.core.particles.ParticleOptions;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.tags.TagKey;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.InsideBlockEffectApplier;
import net.minecraft.world.item.Item;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.StateDefinition;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;
import net.minecraft.world.phys.shapes.VoxelShape;

public abstract class Fluid {
    private final FluidState defaultState = new FluidState(this, true);

    public FluidState defaultFluidState() {
        return defaultState;
    }

    protected boolean isEmpty() {
        return false;
    }

    public int getTickDelay(LevelReader level) {
        return 0;
    }
    public static final IdMapper<FluidState> FLUID_STATE_REGISTRY = null;
    protected final StateDefinition<Fluid, FluidState> stateDefinition = null;
    protected Fluid() {

    }
    private StateDefinition<Fluid, FluidState> definition;
    protected void createFluidStateDefinition(StateDefinition.Builder<Fluid, FluidState> builder) {
    }
    public synchronized StateDefinition<Fluid, FluidState> getStateDefinition() {
        if (definition == null) {
            StateDefinition.Builder<Fluid, FluidState> builder = new StateDefinition.Builder<>(this);
            createFluidStateDefinition(builder);
            definition = builder.create(Fluid::defaultFluidState, FluidState::new);
        }
        return definition;
    }
    protected void registerDefaultState(FluidState state) {

    }
    public Item getBucket() {
        var key = net.minecraft.core.registries.BuiltInRegistries.FLUID.getKey(this);
        String path = key.getPath().startsWith("flowing_") ? key.getPath().substring(8) : key.getPath();
        return net.minecraft.core.registries.BuiltInRegistries.ITEM.getOptional(net.minecraft.resources.Identifier.withDefaultNamespace(path + "_bucket"))
            .orElse(net.minecraft.world.item.Items.AIR);
    }
    protected void animateTick(Level level, BlockPos pos, FluidState fluidState, RandomSource random) {
        
    }
    protected void tick(ServerLevel level, BlockPos pos, BlockState blockState, FluidState fluidState) {
        fluidState.tick(level, pos, blockState);
    }
    protected void randomTick(ServerLevel level, BlockPos pos, FluidState fluidState, RandomSource random) {
        fluidState.randomTick(level, pos, random);
    }
    protected void entityInside(Level level, BlockPos pos, Entity entity, InsideBlockEffectApplier effectApplier) {
        
    }
    protected ParticleOptions getDripParticle() {
        return null;
    }
    protected boolean canBeReplacedWith(FluidState state, BlockGetter level, BlockPos pos, Fluid other, Direction direction) {
        return state.canBeReplacedWith(level, pos, other, direction);
    }
    protected Vec3 getFlow(BlockGetter level, BlockPos pos, FluidState fluidState) {
        return fluidState.getFlow(level, pos);
    }
    protected boolean isRandomlyTicking() {
        return this == Fluids.LAVA || this == Fluids.FLOWING_LAVA;
    }
    protected float getExplosionResistance() {
        return 100.0F;
    }
    public float getHeight(FluidState fluidState, BlockGetter level, BlockPos pos) {
        return fluidState.getHeight(level, pos);
    }
    public float getOwnHeight(FluidState fluidState) {
        return fluidState.getOwnHeight();
    }
    protected BlockState createLegacyBlock(FluidState fluidState) {
        return fluidState.createLegacyBlock();
    }
    public boolean isSource(FluidState fluidState) {
        return fluidState.isSource();
    }
    public int getAmount(FluidState fluidState) {
        return fluidState.getAmount();
    }
    public boolean isSame(Fluid other) {
        return other == this || other.defaultFluidState().is(this) || defaultFluidState().is(other);
    }
    public boolean is(TagKey<Fluid> tag) {
        return defaultFluidState().is(tag);
    }
    public VoxelShape getShape(FluidState state, BlockGetter level, BlockPos pos) {
        return state.getShape(level, pos);
    }
    public AABB getAABB(FluidState state, BlockGetter level, BlockPos pos) {
        return state.getAABB(level, pos);
    }
    public Optional<SoundEvent> getPickupSound() {
        String path = net.minecraft.core.registries.BuiltInRegistries.FLUID.getKey(this).getPath();
        if (path.endsWith("water")) {
            return Optional.of(net.minecraft.sounds.SoundEvents.byId("item.bucket.fill"));
        }
        return path.endsWith("lava") ? Optional.of(net.minecraft.sounds.SoundEvents.byId("item.bucket.fill_lava")) : Optional.empty();
    }
    public Holder.Reference<Fluid> builtInRegistryHolder() {
        return (Holder.Reference<Fluid>) net.minecraft.core.registries.BuiltInRegistries.FLUID.wrapAsHolder(this);
    }
}
