// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.ai.util.GoalUtils;
import net.minecraft.world.level.block.DoorBlock;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.pathfinder.Node;
import net.minecraft.world.level.pathfinder.Path;

public abstract class DoorInteractGoal extends Goal {
    protected Mob mob;
    protected BlockPos doorPos = BlockPos.ZERO;
    protected boolean hasDoor;
    private boolean passed;
    private float doorOpenDirX;
    private float doorOpenDirZ;

    public DoorInteractGoal(Mob mob) {
        this.mob = mob;
    }

    protected boolean isOpen() {
        if (!hasDoor) {
            return false;
        }
        BlockState state = mob.level().getBlockState(doorPos);
        return !(state.getBlock() instanceof DoorBlock door) || door.isOpen(state);
    }

    protected void setOpen(boolean open) {
        if (hasDoor) {
            BlockState state = mob.level().getBlockState(doorPos);
            if (state.getBlock() instanceof DoorBlock door) {
                door.setOpen(mob, mob.level(), state, doorPos, open);
            }
        }
    }

    @Override
    public boolean canUse() {
        if (!GoalUtils.hasGroundPathNavigation(mob) || !mob.horizontalCollision) {
            return false;
        }
        Path path = mob.getNavigation().getPath();
        if (path == null || path.isDone()) {
            return false;
        }
        for (int i = 0; i < Math.min(path.getNextNodeIndex() + 2, path.getNodeCount()); i++) {
            Node node = path.getNode(i);
            doorPos = new BlockPos(node.x, node.y + 1, node.z);
            if (mob.distanceToSqr(doorPos.getX(), mob.getY(), doorPos.getZ()) <= 2.25
                    && DoorBlock.isWoodenDoor(mob.level(), doorPos)) {
                hasDoor = true;
                return true;
            }
        }
        doorPos = mob.blockPosition().above();
        hasDoor = DoorBlock.isWoodenDoor(mob.level(), doorPos);
        return hasDoor;
    }

    @Override
    public boolean canContinueToUse() {
        return !passed;
    }

    @Override
    public void start() {
        passed = false;
        doorOpenDirX = (float) (doorPos.getX() + 0.5 - mob.getX());
        doorOpenDirZ = (float) (doorPos.getZ() + 0.5 - mob.getZ());
    }

    @Override
    public boolean requiresUpdateEveryTick() {
        return true;
    }

    @Override
    public void tick() {
        float dx = (float) (doorPos.getX() + 0.5 - mob.getX());
        float dz = (float) (doorPos.getZ() + 0.5 - mob.getZ());
        if (doorOpenDirX * dx + doorOpenDirZ * dz < 0.0F) {
            passed = true;
        }
    }
}
