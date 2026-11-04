// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.navigation;

import net.minecraft.core.BlockPos;
import net.minecraft.util.Mth;
import net.minecraft.world.level.pathfinder.Node;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.level.Level;
import java.util.Set;
import net.minecraft.world.entity.ai.attributes.Attributes;
import net.minecraft.world.level.PathNavigationRegion;
import net.minecraft.world.level.pathfinder.Path;
import net.minecraft.world.level.pathfinder.PathFinder;
import net.minecraft.world.phys.Vec3;

public abstract class PathNavigation {
    protected final Mob mob;
    protected final Level level;
    protected Path path;
    protected double speedModifier;
    protected int tick;
    protected boolean hasDelayedRecomputation;
    private boolean canFloat;
    protected PathFinder pathFinder;
    private boolean pendingCanOpenDoors;
    protected net.minecraft.world.level.pathfinder.NodeEvaluator nodeEvaluator;
    protected float maxVisitedNodesMultiplier = 1.0F;
    protected float requiredPathLength = 6.0F;
    protected int maxVisitedNodes = 0;
    private BlockPos targetPos;

    public PathNavigation(Mob mob, Level level) {
        this.mob = mob;
        this.level = level;
    }

    private com.destroystokyo.paper.entity.Pathfinder pathfinder() {
        return mob.live() instanceof org.bukkit.entity.Mob living ? living.getPathfinder() : null;
    }

    public BlockPos getTargetPos() {
        return targetPos;
    }

    public void setSpeedModifier(double speedModifier) {
        this.speedModifier = speedModifier;
    }

    public void recomputePath() {
        if (targetPos != null) {
            moveTo(targetPos.getX() + 0.5, targetPos.getY(), targetPos.getZ() + 0.5, speedModifier);
        }
    }

    protected PathFinder createPathFinder(int maxVisitedNodes) {
        throw new UnsupportedOperationException(getClass().getName() + " has no node evaluator");
    }

    protected boolean canUpdatePath() {
        return mob.onGround() || mob.isInLiquid() || mob.isPassenger();
    }

    protected Vec3 getTempMobPos() {
        return mob.position();
    }

    public Path createPath(BlockPos pos, int reachRange) {
        return createPath(Set.of(pos.immutable()), 8, false, reachRange);
    }

    public Path createPath(BlockPos pos, int reachRange, int maxPathLength) {
        return createPath(Set.of(pos.immutable()), 8, false, reachRange, (float) maxPathLength);
    }
    public Path createPath(Set<BlockPos> positions, int reachRange) {
        return createPath(positions, 8, false, reachRange);
    }
    public Path createPath(Entity target, int reachRange) {
        return createPath(Set.of(target.blockPosition()), 16, true, reachRange);
    }

    protected Path createPath(Set<BlockPos> targets, int radiusOffset, boolean above, int reachRange) {
        return createPath(targets, radiusOffset, above, reachRange, (float) mob.getAttributeValue(Attributes.FOLLOW_RANGE));
    }

    protected Path createPath(Set<BlockPos> targets, int radiusOffset, boolean above, int reachRange, float followRange) {
        if (targets.isEmpty() || mob.getY() < level.getMinY() || !canUpdatePath()) {
            return null;
        }
        if (pathFinder == null) {
            if (maxVisitedNodes == 0) {
                maxVisitedNodes = Mth.floor(mob.getAttributeValue(Attributes.FOLLOW_RANGE) * 16.0);
            }
            pathFinder = createPathFinder(maxVisitedNodes);
        }
        BlockPos origin = above ? mob.blockPosition().above() : mob.blockPosition();
        int range = (int) (followRange + radiusOffset);
        PathNavigationRegion region = new PathNavigationRegion(level, origin.offset(-range, -range, -range), origin.offset(range, range, range));
        return pathFinder.findPath(region, mob, targets, followRange, reachRange, maxVisitedNodesMultiplier);
    }

    public Path createPath(double x, double y, double z, int reachRange) {
        return createPath(BlockPos.containing(x, y, z), reachRange);
    }

    public void setCanOpenDoors(boolean canOpenDoors) {
        if (nodeEvaluator != null) {
            nodeEvaluator.setCanOpenDoors(canOpenDoors);
        }
        pendingCanOpenDoors = canOpenDoors;
    }

    public boolean canOpenDoors() {
        return nodeEvaluator != null ? nodeEvaluator.canOpenDoors() : pendingCanOpenDoors;
    }

    public boolean moveTo(Path newPath, double speedModifier) {
        if (newPath == null || newPath.getNodeCount() == 0) {
            return false;
        }
        Node end = newPath.getEndNode();
        boolean started = moveTo(end.x + 0.5, end.y, end.z + 0.5, speedModifier);
        if (started) {
            path = newPath;
        }
        return started;
    }

    public boolean moveTo(double x, double y, double z, double speedModifier) {
        com.destroystokyo.paper.entity.Pathfinder pathfinder = pathfinder();
        if (pathfinder == null) {
            return false;
        }
        this.speedModifier = speedModifier;
        targetPos = BlockPos.containing(x, y, z);
        return pathfinder.moveTo(new org.bukkit.Location(mob.live().getWorld(), x, y, z), speedModifier);
    }

    public boolean moveTo(double x, double y, double z, int reachRange, double speedModifier) {
        return moveTo(x, y, z, speedModifier);
    }

    public boolean moveTo(Entity target, double speedModifier) {
        return moveTo(target.getX(), target.getY(), target.getZ(), speedModifier);
    }

    public boolean moveTo(Entity target, int reachRange, double speedModifier) {
        return moveTo(target, speedModifier);
    }

    public Path getPath() {
        return path;
    }

    public void tick() {
        tick++;
    }

    public boolean isDone() {
        com.destroystokyo.paper.entity.Pathfinder pathfinder = pathfinder();
        return pathfinder == null || !pathfinder.hasPath();
    }

    public boolean isInProgress() {
        return !isDone();
    }

    public void stop() {
        com.destroystokyo.paper.entity.Pathfinder pathfinder = pathfinder();
        if (pathfinder != null) {
            pathfinder.stopPathfinding();
        }
        targetPos = null;
    }

    public void setCanFloat(boolean canFloat) {
        this.canFloat = canFloat;
        if (nodeEvaluator != null) {
            nodeEvaluator.setCanFloat(canFloat);
        }
        com.destroystokyo.paper.entity.Pathfinder pathfinder = pathfinder();
        if (pathfinder != null) {
            pathfinder.setCanFloat(canFloat);
        }
    }

    public boolean canFloat() {
        return canFloat;
    }

    public boolean canNavigateGround() {
        return true;
    }

    public boolean isStableDestination(BlockPos pos) {
        return !level.getBlockState(pos.below()).isAir();
    }

    public boolean shouldRecomputePath(BlockPos pos) {
        return false;
    }

    public float getMaxDistanceToWaypoint() {
        return Math.max(0.5F, mob.getBbWidth());
    }

    public float getMaxVerticalDistanceToWaypoint() {
        return 1.0F;
    }

    public void updatePathfinderMaxVisitedNodes() {
        maxVisitedNodes = Mth.floor(requiredPathLength * 16.0F);
        if (pathFinder != null) {
            pathFinder.setMaxVisitedNodes(maxVisitedNodes);
        }
    }

    public void setRequiredPathLength(float length) {
        requiredPathLength = length;
        updatePathfinderMaxVisitedNodes();
    }

    public void resetMaxVisitedNodesMultiplier() {
        maxVisitedNodesMultiplier = 1.0F;
    }

    public void setMaxVisitedNodesMultiplier(float maxVisitedNodesMultiplier) {
        this.maxVisitedNodesMultiplier = maxVisitedNodesMultiplier;
    }

    public boolean canCutCorner(net.minecraft.world.level.pathfinder.PathType pathType) {
        return pathType != net.minecraft.world.level.pathfinder.PathType.FIRE && pathType != net.minecraft.world.level.pathfinder.PathType.DAMAGING && pathType != net.minecraft.world.level.pathfinder.PathType.WALKABLE_DOOR;
    }
}
