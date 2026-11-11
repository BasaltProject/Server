// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.vehicle;
import net.minecraft.world.entity.EntityTypes;
import net.minecraft.tags.BlockTags;
import net.minecraft.world.level.block.TrapDoorBlock;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.phys.shapes.Shapes;
import java.util.function.Function;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.CollisionGetter;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;
import net.minecraft.world.phys.shapes.VoxelShape;
public class DismountHelper {
    public static int[][] offsetsForDirection(Direction forward) {
        Direction right = forward.getClockWise();
        Direction left = right.getOpposite();
        Direction back = forward.getOpposite();
        return new int[][] {
            {right.getStepX(), right.getStepZ()},
            {left.getStepX(), left.getStepZ()},
            {back.getStepX() + right.getStepX(), back.getStepZ() + right.getStepZ()},
            {back.getStepX() + left.getStepX(), back.getStepZ() + left.getStepZ()},
            {forward.getStepX() + right.getStepX(), forward.getStepZ() + right.getStepZ()},
            {forward.getStepX() + left.getStepX(), forward.getStepZ() + left.getStepZ()},
            {back.getStepX(), back.getStepZ()},
            {forward.getStepX(), forward.getStepZ()}
        };
    }
    public static boolean isBlockFloorValid(double blockFloorHeight) {
        return !Double.isInfinite(blockFloorHeight) && blockFloorHeight < 1.0;
    }
    public static boolean canDismountTo(CollisionGetter level, LivingEntity passenger, AABB box) {
        for (VoxelShape hit : level.getBlockCollisions(passenger, box)) {
            if (!hit.isEmpty()) {
                return false;
            }
        }
        return level.getWorldBorder().isWithinBounds(box);
    }
    public static boolean canDismountTo(CollisionGetter level, Vec3 location, LivingEntity passenger, Pose dismountPose) {
        return canDismountTo(level, passenger, passenger.getLocalBoundsForPose(dismountPose).move(location));
    }
    public static VoxelShape nonClimbableShape(BlockGetter level, BlockPos pos) {
        BlockState state = level.getBlockState(pos);
        boolean openTrapdoor = state.getBlock() instanceof TrapDoorBlock && state.getValue(TrapDoorBlock.OPEN);
        return state.is(BlockTags.CLIMBABLE) || openTrapdoor ? Shapes.empty() : state.getCollisionShape(level, pos);
    }
    public static double findCeilingFrom(BlockPos pos, int blocks, Function<BlockPos, VoxelShape> shapeGetter) {
        BlockPos.MutableBlockPos probe = pos.mutable();
        for (int offset = 0; offset < blocks; offset++, probe.move(Direction.UP)) {
            VoxelShape shape = shapeGetter.apply(probe);
            if (!shape.isEmpty()) {
                return pos.getY() + offset + shape.min(Direction.Axis.Y);
            }
        }
        return Double.POSITIVE_INFINITY;
    }
    public static Vec3 findSafeDismountLocation(EntityType<?> type, CollisionGetter level, BlockPos blockPos, boolean checkDangerous) {
        if (checkDangerous && type.isBlockDangerous(level.getBlockState(blockPos))) {
            return null;
        }
        double floor = level.getBlockFloorHeight(nonClimbableShape(level, blockPos), () -> nonClimbableShape(level, blockPos.below()));
        if (!isBlockFloorValid(floor)) {
            return null;
        }
        if (checkDangerous && floor <= 0.0 && type.isBlockDangerous(level.getBlockState(blockPos.below()))) {
            return null;
        }
        Vec3 spot = Vec3.upFromBottomCenterOf(blockPos, floor);
        AABB box = type.getDimensions().makeBoundingBox(spot);
        for (VoxelShape hit : level.getBlockCollisions(null, box)) {
            if (!hit.isEmpty()) {
                return null;
            }
        }
        boolean badSpawn = type == EntityTypes.PLAYER && (level.getBlockState(blockPos).is(BlockTags.INVALID_SPAWN_INSIDE)
            || level.getBlockState(blockPos.above()).is(BlockTags.INVALID_SPAWN_INSIDE));
        return badSpawn || !level.getWorldBorder().isWithinBounds(box) ? null : spot;


    }
}
