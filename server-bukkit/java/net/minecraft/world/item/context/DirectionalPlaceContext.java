// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.context;

import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.Vec3;

public class DirectionalPlaceContext extends BlockPlaceContext {
    private final Direction direction;

    public DirectionalPlaceContext(Level level, BlockPos pos, Direction direction, ItemStack dispensed, Direction clickedFace) {
        super(level, null, InteractionHand.MAIN_HAND, dispensed,
            new BlockHitResult(Vec3.atBottomCenterOf(pos), clickedFace, pos, false));
        this.direction = direction;
    }

    @Override
    public BlockPos getClickedPos() {
        return getHitResult().getBlockPos();
    }

    @Override
    public boolean canPlace() {
        return getLevel().getBlockState(getHitResult().getBlockPos()).canBeReplaced(this);
    }

    @Override
    public boolean replacingClickedOnBlock() {
        return canPlace();
    }

    @Override
    public Direction getNearestLookingDirection() {
        return Direction.DOWN;
    }

    @Override
    public Direction[] getNearestLookingDirections() {
        return switch (direction) {
            case UP -> new Direction[] {Direction.DOWN, Direction.UP, Direction.NORTH, Direction.EAST, Direction.SOUTH, Direction.WEST};
            case NORTH -> new Direction[] {Direction.DOWN, Direction.NORTH, Direction.EAST, Direction.WEST, Direction.UP, Direction.SOUTH};
            case SOUTH -> new Direction[] {Direction.DOWN, Direction.SOUTH, Direction.EAST, Direction.WEST, Direction.UP, Direction.NORTH};
            case WEST -> new Direction[] {Direction.DOWN, Direction.WEST, Direction.SOUTH, Direction.UP, Direction.NORTH, Direction.EAST};
            case EAST -> new Direction[] {Direction.DOWN, Direction.EAST, Direction.SOUTH, Direction.UP, Direction.NORTH, Direction.WEST};
            default -> new Direction[] {Direction.DOWN, Direction.NORTH, Direction.EAST, Direction.SOUTH, Direction.WEST, Direction.UP};
        };
    }

    @Override
    public Direction getHorizontalDirection() {
        return direction.getAxis() == Direction.Axis.Y ? Direction.NORTH : direction;
    }

    @Override
    public boolean isSecondaryUseActive() {
        return false;
    }

    @Override
    public float getRotation() {
        return direction.get2DDataValue() * 90;
    }
}
