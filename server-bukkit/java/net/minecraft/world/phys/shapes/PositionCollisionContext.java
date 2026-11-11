// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.phys.shapes;
import net.minecraft.core.Direction;
import net.minecraft.core.BlockPos;
import net.minecraft.world.item.Item;
import net.minecraft.world.level.CollisionGetter;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.material.FluidState;
public class PositionCollisionContext implements CollisionContext {
    private final double y;
    public PositionCollisionContext(double y) {
        this.y = y;
    }
    public boolean isDescending() {
        return false;
    }
    public boolean isAbove(VoxelShape shape, BlockPos pos, boolean defaultValue) {
        return y > pos.getY() + shape.max(Direction.Axis.Y) - 1.0E-5F;
    }
    public boolean isHoldingItem(Item item) {
        return false;
    }
    public boolean alwaysCollideWithFluid() {
        return false;
    }
    public boolean canStandOnFluid(FluidState fluidStateAbove, FluidState fluid) {
        return false;
    }
    public VoxelShape getCollisionShape(BlockState state, CollisionGetter collisionGetter, BlockPos pos) {
        return state.getCollisionShape(collisionGetter, pos, this);


    }
}
