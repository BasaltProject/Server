// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.phys.shapes;
import net.minecraft.world.level.block.BaseRailBlock;
import net.minecraft.world.level.block.state.properties.RailShape;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.vehicle.minecart.AbstractMinecart;
import net.minecraft.world.level.CollisionGetter;
import net.minecraft.world.level.block.state.BlockState;
public class MinecartCollisionContext extends EntityCollisionContext {
    private BlockPos ingoreBelow;
    private BlockPos slopeIgnore;
    protected MinecartCollisionContext(AbstractMinecart entity, boolean alwaysStandOnFluid) {
        super(entity, alwaysStandOnFluid, false);
        BlockPos rail = entity.getCurrentBlockPosOrRailBelow();
        BlockState railState = entity.level().getBlockState(rail);
        if (BaseRailBlock.isRail(railState)) {
            this.ingoreBelow = rail.below();
            RailShape railShape = railState.getValue(((BaseRailBlock) railState.getBlock()).getShapeProperty());
            if (railShape.isSlope()) {
                this.slopeIgnore = switch (railShape) {
                    case ASCENDING_EAST -> rail.east();
                    case ASCENDING_WEST -> rail.west();
                    case ASCENDING_NORTH -> rail.north();
                    case ASCENDING_SOUTH -> rail.south();
                    default -> null;
                };
            }
        }
    }
    public VoxelShape getCollisionShape(BlockState state, CollisionGetter collisionGetter, BlockPos pos) {
        return !pos.equals(ingoreBelow) && !pos.equals(slopeIgnore) ? super.getCollisionShape(state, collisionGetter, pos) : Shapes.empty();
    }
}
