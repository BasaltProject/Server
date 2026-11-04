// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.PathfinderMob;

public class MeleeAttackGoal extends Goal {
    protected final PathfinderMob mob;
    private final double speedModifier;
    private final boolean followingTargetEvenIfNotSeen;
    private int ticksUntilNextPathRecalculation;
    private int ticksUntilNextAttack;
    private long lastCanUseCheck;

    public MeleeAttackGoal(PathfinderMob mob, double speedModifier, boolean followingTargetEvenIfNotSeen) {
        this.mob = mob;
        this.speedModifier = speedModifier;
        this.followingTargetEvenIfNotSeen = followingTargetEvenIfNotSeen;
        setFlags(EnumSet.of(Goal.Flag.MOVE, Goal.Flag.LOOK));
    }

    @Override
    public boolean canUse() {
        long now = mob.level().getGameTime();
        if (now - lastCanUseCheck < 20L) {
            return false;
        }
        lastCanUseCheck = now;
        LivingEntity target = mob.getTarget();
        return target != null && target.isAlive();
    }

    @Override
    public boolean canContinueToUse() {
        LivingEntity target = mob.getTarget();
        return target != null && target.isAlive() && (followingTargetEvenIfNotSeen || mob.getSensing().hasLineOfSight(target));
    }

    @Override
    public void start() {
        ticksUntilNextPathRecalculation = 0;
        ticksUntilNextAttack = 0;
    }

    @Override
    public void stop() {
        mob.getNavigation().stop();
    }

    @Override
    public boolean requiresUpdateEveryTick() {
        return true;
    }

    @Override
    public void tick() {
        LivingEntity target = mob.getTarget();
        if (target == null) {
            return;
        }
        mob.getLookControl().setLookAt(target, 30.0F, 30.0F);
        ticksUntilNextPathRecalculation = Math.max(ticksUntilNextPathRecalculation - 1, 0);
        if (ticksUntilNextPathRecalculation <= 0) {
            ticksUntilNextPathRecalculation = 4 + mob.getRandom().nextInt(7);
            mob.getNavigation().moveTo(target, speedModifier);
        }
        ticksUntilNextAttack = Math.max(ticksUntilNextAttack - 1, 0);
        checkAndPerformAttack(target);
    }

    protected void checkAndPerformAttack(LivingEntity target) {
        if (canPerformAttack(target)) {
            resetAttackCooldown();
            mob.swingForAttack(net.minecraft.world.InteractionHand.MAIN_HAND);
            mob.doHurtTarget((net.minecraft.server.level.ServerLevel) mob.level(), target);
        }
    }

    protected void resetAttackCooldown() {
        ticksUntilNextAttack = adjustedTickDelay(20);
    }

    protected boolean isTimeToAttack() {
        return ticksUntilNextAttack <= 0;
    }

    protected boolean canPerformAttack(LivingEntity target) {
        return isTimeToAttack() && mob.isWithinMeleeAttackRange(target) && mob.getSensing().hasLineOfSight(target);
    }

    protected int getTicksUntilNextAttack() {
        return ticksUntilNextAttack;
    }

    protected int getAttackInterval() {
        return adjustedTickDelay(20);
    }
}
