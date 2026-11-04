// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.context;

import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.Vec3;

public class BlockPlaceContext extends UseOnContext {
    private final BlockPos relativePos;
    protected boolean replaceClicked;

    public BlockPlaceContext(Level level, Player player, InteractionHand interactionHand, ItemStack itemStack, BlockHitResult blockHitResult) {
        super(level, player, interactionHand, itemStack, blockHitResult);
        this.relativePos = blockHitResult.getBlockPos().relative(blockHitResult.getDirection());
        this.replaceClicked = true;
        this.replaceClicked = level.getBlockState(blockHitResult.getBlockPos()).canBeReplaced(this);
    }

    public BlockPlaceContext(Player player, InteractionHand hand, ItemStack itemInHand, BlockHitResult hitResult) {
        this(player.level(), player, hand, itemInHand, hitResult);
    }

    public BlockPlaceContext(UseOnContext context) {
        this(context.getLevel(), context.getPlayer(), context.getHand(), context.getItemInHand(), context.getHitResult());
    }

    public static BlockPlaceContext at(BlockPlaceContext context, BlockPos pos, Direction direction) {
        Vec3 center = new Vec3(pos.getX() + 0.5 + direction.getStepX() * 0.5, pos.getY() + 0.5 + direction.getStepY() * 0.5,
            pos.getZ() + 0.5 + direction.getStepZ() * 0.5);
        return new BlockPlaceContext(context.getLevel(), context.getPlayer(), context.getHand(), context.getItemInHand(),
            new BlockHitResult(center, direction, pos, false));
    }

    @Override
    public BlockPos getClickedPos() {
        return replaceClicked ? super.getClickedPos() : relativePos;
    }

    public boolean canPlace() {
        return replaceClicked || getLevel().getBlockState(getClickedPos()).canBeReplaced(this);
    }

    public boolean replacingClickedOnBlock() {
        return replaceClicked;
    }

    public Direction getNearestLookingDirection() {
        return Direction.orderedByNearest(getPlayer())[0];
    }

    public Direction getNearestLookingVerticalDirection() {
        return Direction.getFacingAxis(getPlayer(), Direction.Axis.Y);
    }

    public Direction[] getNearestLookingDirections() {
        Direction[] ordered = Direction.orderedByNearest(getPlayer());
        if (replaceClicked) {
            return ordered;
        }
        Direction behind = getClickedFace().getOpposite();
        int at = 0;
        while (at < ordered.length && ordered[at] != behind) {
            at++;
        }
        if (at > 0 && at < ordered.length) {
            System.arraycopy(ordered, 0, ordered, 1, at);
            ordered[0] = behind;
        }
        return ordered;
    }
}
