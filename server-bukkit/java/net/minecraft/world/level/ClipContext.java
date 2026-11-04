// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.phys.Vec3;
import net.minecraft.world.phys.shapes.CollisionContext;
import net.minecraft.world.phys.shapes.VoxelShape;

public class ClipContext {
    private final Vec3 from;
    private final Vec3 to;
    private final Block block;
    private final Fluid fluid;
    private final CollisionContext collisionContext;

    protected ClipContext() {
        this(Vec3.ZERO, Vec3.ZERO, Block.COLLIDER, Fluid.NONE, (CollisionContext) null);
    }

    public ClipContext(Vec3 from, Vec3 to, Block block, Fluid fluid, Entity entity) {
        this(from, to, block, fluid, (CollisionContext) null);
    }

    public ClipContext(Vec3 from, Vec3 to, Block block, Fluid fluid, CollisionContext collisionContext) {
        this.from = from;
        this.to = to;
        this.block = block;
        this.fluid = fluid;
        this.collisionContext = collisionContext;
    }

    public Vec3 getTo() {
        return to;
    }

    public Vec3 getFrom() {
        return from;
    }

    public Block blockMode() {
        return block;
    }

    public Fluid fluidMode() {
        return fluid;
    }

    public VoxelShape getBlockShape(BlockState blockState, BlockGetter level, BlockPos pos) {
        return block.get(blockState, level, pos, collisionContext);
    }

    public VoxelShape getFluidShape(FluidState fluidState, BlockGetter level, BlockPos pos) {
        return fluid.canPick(fluidState) ? fluidState.getShape(level, pos) : VoxelShape.EMPTY;
    }

    public static enum Block implements ShapeGetter {
        COLLIDER, OUTLINE, VISUAL, FALLDAMAGE_RESETTING;

        public VoxelShape get(BlockState state, BlockGetter level, BlockPos pos, CollisionContext context) {
            return switch (this) {
                case COLLIDER -> context == null ? state.getCollisionShape(level, pos) : state.getCollisionShape(level, pos, context);
                case OUTLINE -> context == null ? state.getShape(level, pos) : state.getShape(level, pos, context);
                case VISUAL -> state.getVisualShape(level, pos, context);
                case FALLDAMAGE_RESETTING -> state.getCollisionShape(level, pos);
            };
        }
    }

    public static enum Fluid {
        NONE, SOURCE_ONLY, ANY, WATER;

        public boolean canPick(FluidState fluidState) {
            return switch (this) {
                case NONE -> false;
                case SOURCE_ONLY -> fluidState.isSource();
                case ANY -> !fluidState.isEmpty();
                case WATER -> fluidState.is(net.minecraft.tags.FluidTags.WATER);
            };
        }
    }

    public interface ShapeGetter {
        VoxelShape get(BlockState state, BlockGetter level, BlockPos pos, CollisionContext context);
    }
}
