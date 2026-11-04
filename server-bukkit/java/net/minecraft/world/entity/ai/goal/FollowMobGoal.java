// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import java.util.List;
import java.util.function.Predicate;
import net.minecraft.world.entity.Mob;

public class FollowMobGoal extends Goal {
    private final Mob mob;
    private final Predicate<Mob> followPredicate;
    private Mob followingMob;
    private final double speedModifier;
    private int timeToRecalcPath;
    private final float stopDistance;
    private final float areaSize;

    public FollowMobGoal(Mob mob, double speedModifier, float stopDistance, float areaSize) {
        this.mob = mob;
        this.followPredicate = other -> other != null && mob.getClass() != other.getClass();
        this.speedModifier = speedModifier;
        this.stopDistance = stopDistance;
        this.areaSize = areaSize;
        setFlags(EnumSet.of(Goal.Flag.MOVE, Goal.Flag.LOOK));
    }

    @Override
    public boolean canUse() {
        List<Mob> nearby = mob.level().getEntitiesOfClass(Mob.class, mob.getBoundingBox().inflate(areaSize), followPredicate);
        for (Mob candidate : nearby) {
            if (!candidate.isInvisible()) {
                followingMob = candidate;
                return true;
            }
        }
        return false;
    }

    @Override
    public boolean canContinueToUse() {
        return followingMob != null && !mob.getNavigation().isDone() && mob.distanceToSqr(followingMob) > (double) (stopDistance * stopDistance);
    }

    @Override
    public void start() {
        timeToRecalcPath = 0;
    }

    @Override
    public void stop() {
        followingMob = null;
        mob.getNavigation().stop();
    }

    @Override
    public void tick() {
        if (followingMob == null || mob.isLeashed()) {
            return;
        }
        mob.getLookControl().setLookAt(followingMob, 10.0F, (float) mob.getMaxHeadXRot());
        if (--timeToRecalcPath <= 0) {
            timeToRecalcPath = adjustedTickDelay(10);
            double dx = mob.getX() - followingMob.getX();
            double dy = mob.getY() - followingMob.getY();
            double dz = mob.getZ() - followingMob.getZ();
            double distSq = dx * dx + dy * dy + dz * dz;
            if (distSq > (double) (stopDistance * stopDistance)) {
                mob.getNavigation().moveTo(followingMob, speedModifier);
            } else {
                mob.getNavigation().stop();
                if (distSq <= (double) stopDistance) {
                    mob.getMoveControl().setWantedPosition(mob.getX() - dx, mob.getY(), mob.getZ() - dz, speedModifier);
                }
            }
        }
    }
}
