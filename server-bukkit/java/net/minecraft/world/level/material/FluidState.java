// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.material;

import net.minecraft.core.TypedInstance;
import net.minecraft.tags.FluidTags;
import net.minecraft.tags.TagKey;
import net.minecraft.world.level.block.state.StateHolder;
import com.mojang.serialization.Codec;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.Holder;
import net.minecraft.core.particles.ParticleOptions;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.InsideBlockEffectApplier;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.Property;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;
import net.minecraft.world.phys.shapes.VoxelShape;

public class FluidState extends StateHolder<Fluid, FluidState> implements TypedInstance<Fluid> {
    private final boolean source;

    public FluidState(Fluid fluid, boolean source) {
        super(fluid);
        this.source = source;
    }

    public Fluid getType() {
        return owner;
    }

    public boolean isEmpty() {
        return owner.isEmpty();
    }

    public boolean isSource() {
        return source && !owner.isEmpty();
    }

    public boolean is(Fluid fluid) {
        return owner == fluid || fluid == Fluids.WATER && owner == Fluids.FLOWING_WATER
            || fluid == Fluids.LAVA && owner == Fluids.FLOWING_LAVA;
    }

    public boolean is(TagKey<Fluid> tag) {
        if (tag == FluidTags.WATER) {
            return is(Fluids.WATER);
        }
        return tag == FluidTags.LAVA && is(Fluids.LAVA);
    }
    public static final Codec<FluidState> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.material.FluidState.class, "CODEC");
    public static final int AMOUNT_MAX = 9;
    public static final int AMOUNT_FULL = 8;
    public FluidState(Fluid owner, Property<?>[] propertyKeys, Comparable<?>[] propertyValues) {
        super(owner, propertyKeys, propertyValues);
        this.source = true;
    }
    public boolean isSourceOfType(Fluid fluidType) {
        return isSource() && owner == fluidType || isSource() && is(fluidType);
    }
    public float getHeight(BlockGetter level, BlockPos pos) {
        return level.getFluidState(pos.above()).getType() == getType() ? 1.0F : getOwnHeight();
    }
    public float getHeightForCamera(BlockGetter level, BlockPos pos) {
        return level.getFluidState(pos.above()).getType() == getType() ? 1.0F : getOwnHeight();
    }
    public float getOwnHeight() {
        return getAmount() / 9.0F;
    }
    public boolean isFull() {
        return getAmount() == AMOUNT_FULL;
    }
    public int getAmount() {
        return isEmpty() ? 0 : source ? AMOUNT_FULL : 1;
    }
    public boolean shouldRenderBackwardUpFace(BlockGetter level, BlockPos above) {
        return false;
    }
    public void tick(ServerLevel level, BlockPos pos, BlockState blockState) {
        org.basaltmc.bukkit.WorldNatives.scheduleTick(level.getWorld().getName(), pos.getX(), pos.getY(), pos.getZ(), 0, true);
    }
    public void animateTick(Level level, BlockPos pos, RandomSource random) {
        return;
    }
    public boolean isRandomlyTicking() {
        return is(Fluids.LAVA);
    }
    public void randomTick(ServerLevel level, BlockPos pos, RandomSource random) {
        if (isRandomlyTicking()) {
            org.basaltmc.bukkit.RestNatives.randomTick(level.getWorld().getName(), pos.getX(), pos.getY(), pos.getZ());
        }
    }
    public Vec3 getFlow(BlockGetter level, BlockPos pos) {
        if (isEmpty() || !(level instanceof ServerLevel server)) {
            return Vec3.ZERO;
        }
        double[] flow = org.basaltmc.bukkit.WorldEnvNatives.fluidFlow(server.getWorld().getName(), pos.getX(), pos.getY(), pos.getZ());
        return flow == null || flow.length < 3 ? Vec3.ZERO : new Vec3(flow[0], flow[1], flow[2]);
    }
    public BlockState createLegacyBlock() {
        if (isEmpty()) {
            return net.minecraft.world.level.block.Blocks.AIR.defaultBlockState();
        }
        boolean lava = is(Fluids.LAVA);
        int id = org.basaltmc.bukkit.WorldNatives.stateId("minecraft:" + (lava ? "lava" : "water") + "[level=" + (source ? 0 : 8 - getAmount()) + "]");
        BlockState found = id < 0 ? null : net.minecraft.world.level.block.Block.stateById(id);
        return found == null ? net.minecraft.world.level.block.Blocks.AIR.defaultBlockState() : found;
    }
    public ParticleOptions getDripParticle() {
        return null;
    }
    public Holder<Fluid> typeHolder() {
        return net.minecraft.core.registries.BuiltInRegistries.FLUID.wrapAsHolder(getType());
    }
    public float getExplosionResistance() {
        return is(Fluids.LAVA) || is(Fluids.WATER) ? 100.0F : 0.0F;
    }
    public boolean canBeReplacedWith(BlockGetter level, BlockPos pos, Fluid other, Direction direction) {
        return getHeight(level, pos) >= 0.44444445F && (other == Fluids.WATER || other == Fluids.FLOWING_WATER);
    }
    public VoxelShape getShape(BlockGetter level, BlockPos pos) {
        return isFull() && level.getFluidState(pos.above()).getType() == getType() ? VoxelShape.BLOCK
            : VoxelShape.ofBoxes(java.util.List.of(getAABB(level, pos)));
    }
    public AABB getAABB(BlockGetter level, BlockPos pos) {
        return new AABB(0.0, 0.0, 0.0, 1.0, getHeight(level, pos), 1.0);
    }
    public void entityInside(Level level, BlockPos pos, Entity entity, InsideBlockEffectApplier effectApplier) {
        return;
    }
}
