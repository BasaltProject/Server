// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.navigation;

import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.pathfinder.*;
import net.minecraft.world.phys.Vec3;

public class GroundPathNavigation extends PathNavigation {
    private boolean avoidSun;
    private boolean canPathToTargetsBelowSurface;

    public GroundPathNavigation(Mob mob, Level level) {
        super(mob, level);
    }

    @Override
    protected PathFinder createPathFinder(int maxVisitedNodes) {
        nodeEvaluator = new WalkNodeEvaluator();
        nodeEvaluator.setCanPassDoors(true);
        return new PathFinder(nodeEvaluator, maxVisitedNodes);
    }

    @Override
    protected Vec3 getTempMobPos() {
        return new Vec3(mob.getX(), getSurfaceY(), mob.getZ());
    }

    private int getSurfaceY() {
        return net.minecraft.util.Mth.floor(mob.getY() + 0.5);
    }

    @Override
    public Path createPath(BlockPos pos, int reachRange) {
        BlockPos target = pos;
        if (!canPathToTargetsBelowSurface && level.getBlockState(pos).isAir()) {
            BlockPos cursor = pos.below();
            while (cursor.getY() > level.getMinY() && level.getBlockState(cursor).isAir()) {
                cursor = cursor.below();
            }
            if (cursor.getY() > level.getMinY()) {
                return super.createPath(cursor.above(), reachRange);
            }
            while (cursor.getY() < level.getMaxY() && level.getBlockState(cursor).isAir()) {
                cursor = cursor.above();
            }
            target = cursor;
        }
        return super.createPath(target, reachRange);
    }

    @Override
    public Path createPath(Entity target, int reachRange) {
        return createPath(target.blockPosition(), reachRange);
    }

    @Override
    public boolean canNavigateGround() {
        return true;
    }

    public boolean avoidSun() {
        return avoidSun;
    }

    public void setAvoidSun(boolean avoidSun) {
        this.avoidSun = avoidSun;
    }

    public void setCanWalkOverFences(boolean canWalkOverFences) {
        if (nodeEvaluator != null) {
            nodeEvaluator.setCanWalkOverFences(canWalkOverFences);
        }
    }

    public void setCanPathToTargetsBelowSurface(boolean canPathToTargetsBelowSurface) {
        this.canPathToTargetsBelowSurface = canPathToTargetsBelowSurface;
    }
}
