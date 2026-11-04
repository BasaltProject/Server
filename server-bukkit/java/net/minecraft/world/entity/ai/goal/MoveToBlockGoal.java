// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.level.LevelReader;

public abstract class MoveToBlockGoal extends Goal {
    protected final PathfinderMob mob;
    public final double speedModifier;
    protected int nextStartTick;
    protected int tryTicks;
    private int maxStayTicks;
    protected BlockPos blockPos = BlockPos.ZERO;
    private boolean reachedTarget;
    private final int searchRange;
    private final int verticalSearchRange;
    protected int verticalSearchStart;

    public MoveToBlockGoal(PathfinderMob mob, double speedModifier, int searchRange) {
        this(mob, speedModifier, searchRange, 1);
    }

    public MoveToBlockGoal(PathfinderMob mob, double speedModifier, int searchRange, int verticalSearchRange) {
        this.mob = mob;
        this.speedModifier = speedModifier;
        this.searchRange = searchRange;
        this.verticalSearchStart = 0;
        this.verticalSearchRange = verticalSearchRange;
        setFlags(EnumSet.of(Goal.Flag.MOVE, Goal.Flag.JUMP));
    }

    @Override
    public boolean canUse() {
        if (nextStartTick > 0) {
            nextStartTick--;
            return false;
        }
        nextStartTick = nextStartTick(mob);
        return findNearestBlock();
    }

    protected int nextStartTick(PathfinderMob mob) {
        return reducedTickDelay(200 + mob.getRandom().nextInt(200));
    }

    @Override
    public boolean canContinueToUse() {
        return tryTicks >= -maxStayTicks && tryTicks <= 1200 && isValidTarget(mob.level(), blockPos);
    }

    @Override
    public void start() {
        moveMobToBlock();
        tryTicks = 0;
        maxStayTicks = mob.getRandom().nextInt(mob.getRandom().nextInt(1200) + 1200) + 1200;
    }

    protected void moveMobToBlock() {
        BlockPos target = getMoveToTarget();
        mob.getNavigation().moveTo(target.getX() + 0.5, target.getY(), target.getZ() + 0.5, speedModifier);
    }

    public double acceptedDistance() {
        return 1.0;
    }

    protected BlockPos getMoveToTarget() {
        return blockPos.above();
    }

    @Override
    public boolean requiresUpdateEveryTick() {
        return true;
    }

    @Override
    public void tick() {
        BlockPos target = getMoveToTarget();
        double accepted = acceptedDistance();
        if (target.distToCenterSqr(mob.getX(), mob.getY(), mob.getZ()) > accepted * accepted) {
            reachedTarget = false;
            tryTicks++;
            if (shouldRecalculatePath()) {
                mob.getNavigation().moveTo(target.getX() + 0.5, target.getY(), target.getZ() + 0.5, speedModifier);
            }
        } else {
            reachedTarget = true;
            tryTicks--;
        }
    }

    public boolean shouldRecalculatePath() {
        return tryTicks % 40 == 0;
    }

    protected boolean isReachedTarget() {
        return reachedTarget;
    }

    protected boolean findNearestBlock() {
        BlockPos origin = mob.blockPosition();
        BlockPos best = null;
        double bestDistance = Double.MAX_VALUE;
        for (int dy = verticalSearchStart; dy <= verticalSearchRange; dy = dy > 0 ? -dy : 1 - dy) {
            for (int radius = 0; radius < searchRange; radius++) {
                for (int dx = -radius; dx <= radius; dx++) {
                    for (int dz = -radius; dz <= radius; dz++) {
                        BlockPos candidate = origin.offset(dx, dy - 1, dz);
                        double distance = candidate.distSqr(origin);
                        if (distance < bestDistance && isValidTarget(mob.level(), candidate)) {
                            best = candidate;
                            bestDistance = distance;
                        }
                    }
                }
            }
        }
        if (best == null) {
            return false;
        }
        blockPos = best;
        return true;
    }

    protected abstract boolean isValidTarget(LevelReader level, BlockPos pos);
}
