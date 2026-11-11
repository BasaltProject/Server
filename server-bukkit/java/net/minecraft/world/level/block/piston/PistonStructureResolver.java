// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.piston;
import java.util.ArrayList;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.material.PushReaction;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.level.Level;
public class PistonStructureResolver {
    private final Level level;
    private final BlockPos pistonPos;
    private final boolean extending;
    private final BlockPos startPos;
    private final Direction pushDirection;
    private final Direction pistonDirection;
    private final List<BlockPos> toPush = new ArrayList<>();
    private final List<BlockPos> toDestroy = new ArrayList<>();
    private static boolean isSticky(BlockState state) {
        return state.is(Blocks.SLIME_BLOCK) || state.is(Blocks.HONEY_BLOCK);
    }
    private static boolean canStickToEachOther(BlockState first, BlockState second) {
        boolean mixed = first.is(Blocks.HONEY_BLOCK) && second.is(Blocks.SLIME_BLOCK) || first.is(Blocks.SLIME_BLOCK) && second.is(Blocks.HONEY_BLOCK);
        return !mixed && (isSticky(first) || isSticky(second));
    }
    private boolean addBlockLine(BlockPos start, Direction direction) {
        BlockState first = level.getBlockState(start);
        if (first.isAir() || !PistonBaseBlock.isPushable(first, level, start, pushDirection, false, direction)
            || start.equals(pistonPos) || toPush.contains(start)) {
            return true;
        }
        int run = 1;
        if (run + toPush.size() > MAX_PUSH_DEPTH) {
            return false;
        }
        BlockState cursor = first;
        while (isSticky(cursor)) {
            BlockPos behind = start.relative(pushDirection.getOpposite(), run);
            BlockState previous = cursor;
            cursor = level.getBlockState(behind);
            if (cursor.isAir() || !canStickToEachOther(previous, cursor)
                || !PistonBaseBlock.isPushable(cursor, level, behind, pushDirection, false, pushDirection.getOpposite()) || behind.equals(pistonPos)) {
                break;
            }
            run++;
            if (run + toPush.size() > MAX_PUSH_DEPTH) {
                return false;
            }
        }
        int added = 0;
        for (int back = run - 1; back >= 0; back--) {
            toPush.add(start.relative(pushDirection.getOpposite(), back));
            added++;
        }
        for (int ahead = 1;; ahead++) {
            BlockPos probe = start.relative(pushDirection, ahead);
            int clash = toPush.indexOf(probe);
            if (clash > -1) {
                reorderAtCollision(added, clash);
                for (int j = 0; j <= clash + added; j++) {
                    BlockPos member = toPush.get(j);
                    if (isSticky(level.getBlockState(member)) && !addBranchingBlocks(member)) {
                        return false;
                    }
                }
                return true;
            }
            BlockState there = level.getBlockState(probe);
            if (there.isAir()) {
                return true;
            }
            if (!PistonBaseBlock.isPushable(there, level, probe, pushDirection, true, pushDirection) || probe.equals(pistonPos)) {
                return false;
            }
            if (there.getPistonPushReaction() == PushReaction.POPPED) {
                toDestroy.add(probe);
                return true;
            }
            if (toPush.size() >= MAX_PUSH_DEPTH) {
                return false;
            }
            toPush.add(probe);
            added++;
        }
    }
    private void reorderAtCollision(int added, int clash) {
        int size = toPush.size();
        List<BlockPos> reordered = new ArrayList<>(toPush.subList(0, clash));
        reordered.addAll(toPush.subList(size - added, size));
        reordered.addAll(toPush.subList(clash, size - added));
        toPush.clear();
        toPush.addAll(reordered);
    }
    private boolean addBranchingBlocks(BlockPos from) {
        BlockState fromState = level.getBlockState(from);
        for (Direction side : Direction.values()) {
            if (side.getAxis() == pushDirection.getAxis()) {
                continue;
            }
            BlockPos neighbour = from.relative(side);
            if (canStickToEachOther(level.getBlockState(neighbour), fromState) && !addBlockLine(neighbour, side)) {
                return false;
            }
        }
        return true;
    }
    public static final int MAX_PUSH_DEPTH = 12;
    public PistonStructureResolver(Level level, BlockPos pistonPos, Direction direction, boolean extending) {
        this.level = level;
        this.pistonPos = pistonPos;
        this.pistonDirection = direction;
        this.extending = extending;
        this.pushDirection = extending ? direction : direction.getOpposite();
        this.startPos = extending ? pistonPos.relative(direction) : pistonPos.relative(direction, 2);
    }
    public boolean resolve() {
        toPush.clear();
        toDestroy.clear();
        BlockState first = level.getBlockState(startPos);
        if (!PistonBaseBlock.isPushable(first, level, startPos, pushDirection, false, pistonDirection)) {
            if (extending && first.getPistonPushReaction() == PushReaction.POPPED) {
                toDestroy.add(startPos);
                return true;
            }
            return false;
        }
        if (!addBlockLine(startPos, pushDirection)) {
            return false;
        }
        for (int i = 0; i < toPush.size(); i++) {
            BlockPos member = toPush.get(i);
            if (isSticky(level.getBlockState(member)) && !addBranchingBlocks(member)) {
                return false;
            }
        }
        return true;
    }
    public Direction getPushDirection() {
        return pushDirection;
    }
    public List<BlockPos> getToPush() {
        return toPush;
    }
    public List<BlockPos> getToDestroy() {
        return toDestroy;


    }
}
