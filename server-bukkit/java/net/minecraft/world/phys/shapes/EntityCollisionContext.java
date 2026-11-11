// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.phys.shapes;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.core.Direction;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.CollisionGetter;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.material.FluidState;
public class EntityCollisionContext implements CollisionContext {
    private final boolean descending;
    private final boolean placement;
    private final double entityBottom;
    private final ItemStack heldItem;
    private final boolean alwaysCollideWithFluid;
    private final Entity entity;
    protected EntityCollisionContext(boolean descending, boolean placement, double entityBottom, ItemStack heldItem, boolean alwaysCollideWithFluid, Entity entity) {
        this.descending = descending;
        this.placement = placement;
        this.entityBottom = entityBottom;
        this.heldItem = heldItem;
        this.alwaysCollideWithFluid = alwaysCollideWithFluid;
        this.entity = entity;
    }
    protected EntityCollisionContext(Entity entity, boolean alwaysCollideWithFluid, boolean placement) {
        this(entity.isDescending(), placement, entity.getY(),
            entity instanceof LivingEntity living ? living.getMainHandItem() : ItemStack.EMPTY, alwaysCollideWithFluid, entity);
    }
    public boolean isHoldingItem(Item item) {
        return heldItem.is(item);
    }
    public boolean alwaysCollideWithFluid() {
        return alwaysCollideWithFluid;
    }
    public boolean canStandOnFluid(FluidState fluidStateAbove, FluidState fluid) {
        return entity instanceof LivingEntity living && living.canStandOnFluid(fluid) && !fluidStateAbove.getType().isSame(fluid.getType());
    }
    public VoxelShape getCollisionShape(BlockState state, CollisionGetter collisionGetter, BlockPos pos) {
        return state.getCollisionShape(collisionGetter, pos, this);
    }
    public boolean isDescending() {
        return descending;
    }
    public boolean isAbove(VoxelShape shape, BlockPos pos, boolean defaultValue) {
        return entityBottom > pos.getY() + shape.max(Direction.Axis.Y) - 1.0E-5F;
    }
    public Entity getEntity() {
        return entity;
    }
    public boolean isPlacement() {
        return placement;
    }
    protected static class Empty extends EntityCollisionContext {
        protected static final CollisionContext WITHOUT_FLUID_COLLISIONS = new Empty(false);
        protected static final CollisionContext WITH_FLUID_COLLISIONS = new Empty(true);
        public Empty(boolean alwaysCollideWithFluid) {
            super(false, false, -Double.MAX_VALUE, ItemStack.EMPTY, alwaysCollideWithFluid, null);
        }
        public boolean isAbove(VoxelShape shape, BlockPos pos, boolean defaultValue) {
            return defaultValue;
        }
    }
}
