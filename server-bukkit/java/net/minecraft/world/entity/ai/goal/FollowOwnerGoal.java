// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.TamableAnimal;

public class FollowOwnerGoal extends Goal {
    private final TamableAnimal tamable;
    private LivingEntity owner;
    private final double speedModifier;
    private final float stopDistance;
    private final float startDistance;
    private int timeToRecalcPath;

    public FollowOwnerGoal(TamableAnimal tamable, double speedModifier, float startDistance, float stopDistance) {
        this.tamable = tamable;
        this.speedModifier = speedModifier;
        this.startDistance = startDistance;
        this.stopDistance = stopDistance;
        setFlags(EnumSet.of(Goal.Flag.MOVE, Goal.Flag.LOOK));
    }

    @Override
    public boolean canUse() {
        LivingEntity candidate = tamable.getOwner();
        if (candidate == null || candidate.isSpectator() || tamable.unableToMoveToOwner()
                || tamable.distanceToSqr(candidate) < (double) (startDistance * startDistance)) {
            return false;
        }
        owner = candidate;
        return true;
    }

    @Override
    public boolean canContinueToUse() {
        return !tamable.getNavigation().isDone() && !tamable.unableToMoveToOwner()
                && tamable.distanceToSqr(owner) > (double) (stopDistance * stopDistance);
    }

    @Override
    public void start() {
        timeToRecalcPath = 0;
    }

    @Override
    public void stop() {
        owner = null;
        tamable.getNavigation().stop();
    }

    @Override
    public boolean requiresUpdateEveryTick() {
        return true;
    }

    @Override
    public void tick() {
        tamable.getLookControl().setLookAt(owner, 10.0F, (float) tamable.getMaxHeadXRot());
        if (--timeToRecalcPath <= 0) {
            timeToRecalcPath = adjustedTickDelay(10);
            if (tamable.shouldTryTeleportToOwner()) {
                tamable.tryToTeleportToOwner();
            } else {
                tamable.getNavigation().moveTo(owner, speedModifier);
            }
        }
    }
}
