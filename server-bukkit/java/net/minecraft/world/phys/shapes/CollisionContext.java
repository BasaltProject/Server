// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.phys.shapes;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.Item;
import net.minecraft.world.level.CollisionGetter;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.material.FluidState;
public interface CollisionContext {
    static CollisionContext empty() {
        return EntityCollisionContext.Empty.WITHOUT_FLUID_COLLISIONS;
    }
    static CollisionContext emptyWithFluidCollisions() {
        return EntityCollisionContext.Empty.WITH_FLUID_COLLISIONS;
    }
    static CollisionContext of(Entity entity) {
        return new EntityCollisionContext(entity, false, false);
    }
    static CollisionContext of(Entity entity, boolean alwaysCollideWithFluid) {
        return new EntityCollisionContext(entity, alwaysCollideWithFluid, false);
    }
    static CollisionContext positionContext(double y) {
        return new PositionCollisionContext(y);
    }
    static CollisionContext placementContext(Player player) {
        boolean sneaking = player != null && player.isDescending();
        double feet = player != null ? player.getY() : -Double.MAX_VALUE;
        ItemStack held = player != null ? player.getMainHandItem() : ItemStack.EMPTY;
        return new EntityCollisionContext(sneaking, true, feet, held, false, player);
    }
    static CollisionContext withPosition(Entity entity, double position) {
        boolean sneaking = entity != null && entity.isDescending();
        double feet = entity != null ? position : -Double.MAX_VALUE;
        ItemStack held = entity instanceof LivingEntity living ? living.getMainHandItem() : ItemStack.EMPTY;
        return new EntityCollisionContext(sneaking, true, feet, held, false, entity);
    }
    boolean isDescending();
    boolean isAbove(VoxelShape shape, BlockPos pos, boolean defaultValue);
    boolean isHoldingItem(Item item);
    boolean alwaysCollideWithFluid();
    boolean canStandOnFluid(FluidState fluidStateAbove, FluidState fluid);
    VoxelShape getCollisionShape(BlockState state, CollisionGetter collisionGetter, BlockPos pos);
    default boolean isPlacement() {
        return false;
    }
}
