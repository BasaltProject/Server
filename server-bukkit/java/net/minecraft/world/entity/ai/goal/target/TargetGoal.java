// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal.target;

import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.ai.attributes.Attributes;
import net.minecraft.world.entity.ai.goal.Goal;
import net.minecraft.world.entity.ai.targeting.TargetingConditions;

public abstract class TargetGoal extends Goal {
    protected final Mob mob;
    protected final boolean mustSee;
    private final boolean mustReach;
    protected LivingEntity targetMob;
    protected int unseenMemoryTicks = 60;
    private int unseenTicks;

    public TargetGoal(Mob mob, boolean mustSee) {
        this(mob, mustSee, false);
    }

    public TargetGoal(Mob mob, boolean mustSee, boolean mustReach) {
        this.mob = mob;
        this.mustSee = mustSee;
        this.mustReach = mustReach;
    }

    @Override
    public boolean canContinueToUse() {
        LivingEntity target = mob.getTarget();
        if (target == null) {
            target = targetMob;
        }
        if (target == null || !mob.canAttack(target)) {
            return false;
        }
        double follow = getFollowDistance();
        if (mob.distanceToSqr(target) > follow * follow) {
            return false;
        }
        if (mustSee) {
            if (mob.getSensing().hasLineOfSight(target)) {
                unseenTicks = 0;
            } else if (++unseenTicks > adjustedTickDelay(unseenMemoryTicks)) {
                return false;
            }
        }
        mob.setTarget(target);
        return true;
    }

    protected double getFollowDistance() {
        return mob.getAttributeValue(Attributes.FOLLOW_RANGE);
    }

    @Override
    public void start() {
        unseenTicks = 0;
    }

    @Override
    public void stop() {
        mob.setTarget(null);
        targetMob = null;
    }

    protected boolean canAttack(LivingEntity target, TargetingConditions targetConditions) {
        return target != null && targetConditions.test((ServerLevel) mob.level(), mob, target);
    }

    public TargetGoal setUnseenMemoryTicks(int unseenMemoryTicks) {
        this.unseenMemoryTicks = unseenMemoryTicks;
        return this;
    }
}
