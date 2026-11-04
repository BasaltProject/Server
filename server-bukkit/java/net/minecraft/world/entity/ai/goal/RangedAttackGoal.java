// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.monster.RangedAttackMob;

public class RangedAttackGoal extends Goal {
    private final Mob mob;
    private final RangedAttackMob rangedMob;
    private LivingEntity target;
    private int attackTime = -1;
    private final double speedModifier;
    private int seeTime;
    private final int attackIntervalMin;
    private final int attackIntervalMax;
    private final float attackRadius;
    private final float attackRadiusSqr;

    public RangedAttackGoal(RangedAttackMob mob, double speedModifier, int attackInterval, float attackRadius) {
        this(mob, speedModifier, attackInterval, attackInterval, attackRadius);
    }

    public RangedAttackGoal(RangedAttackMob mob, double speedModifier, int attackIntervalMin, int attackIntervalMax, float attackRadius) {
        this.rangedMob = mob;
        this.mob = (Mob) mob;
        this.speedModifier = speedModifier;
        this.attackIntervalMin = attackIntervalMin;
        this.attackIntervalMax = attackIntervalMax;
        this.attackRadius = attackRadius;
        this.attackRadiusSqr = attackRadius * attackRadius;
        setFlags(EnumSet.of(Goal.Flag.MOVE, Goal.Flag.LOOK));
    }

    @Override
    public boolean canUse() {
        LivingEntity candidate = mob.getTarget();
        if (candidate == null || !candidate.isAlive()) {
            return false;
        }
        target = candidate;
        return true;
    }

    @Override
    public boolean canContinueToUse() {
        return canUse() || (target != null && target.isAlive() && !mob.getNavigation().isDone());
    }

    @Override
    public void stop() {
        target = null;
        seeTime = 0;
        attackTime = -1;
    }

    @Override
    public boolean requiresUpdateEveryTick() {
        return true;
    }

    @Override
    public void tick() {
        if (target == null) {
            return;
        }
        double distance = mob.distanceToSqr(target.getX(), target.getY(), target.getZ());
        boolean visible = mob.getSensing().hasLineOfSight(target);
        seeTime = visible ? seeTime + 1 : 0;
        if (distance <= attackRadiusSqr && seeTime >= 5) {
            mob.getNavigation().stop();
        } else {
            mob.getNavigation().moveTo(target, speedModifier);
        }
        mob.getLookControl().setLookAt(target, 30.0F, 30.0F);
        if (--attackTime == 0) {
            if (!visible) {
                return;
            }
            float power = (float) Math.sqrt(distance) / attackRadius;
            rangedMob.performRangedAttack(target, net.minecraft.util.Mth.clamp(power, 0.1F, 1.0F));
            attackTime = (int) (net.minecraft.util.Mth.lerp(Math.sqrt(distance) / attackRadius, attackIntervalMin, attackIntervalMax));
        } else if (attackTime < 0) {
            attackTime = (int) (net.minecraft.util.Mth.lerp(Math.sqrt(distance) / attackRadius, attackIntervalMin, attackIntervalMax));
        }
    }
}
