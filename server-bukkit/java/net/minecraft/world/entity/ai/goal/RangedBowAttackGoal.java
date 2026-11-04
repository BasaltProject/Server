// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.monster.Monster;
import net.minecraft.world.entity.monster.RangedAttackMob;

public class RangedBowAttackGoal<T extends Monster & RangedAttackMob> extends Goal {
    private final T mob;
    private final double speedModifier;
    private int attackIntervalMin;
    private final float attackRadiusSqr;
    private int attackTime = -1;
    private int seeTime;

    public RangedBowAttackGoal(T mob, double speedModifier, int attackIntervalMin, float attackRadius) {
        this.mob = mob;
        this.speedModifier = speedModifier;
        this.attackIntervalMin = attackIntervalMin;
        this.attackRadiusSqr = attackRadius * attackRadius;
        setFlags(EnumSet.of(Goal.Flag.MOVE, Goal.Flag.LOOK));
    }

    public void setMinAttackInterval(int ticks) {
        this.attackIntervalMin = ticks;
    }

    @Override
    public boolean canUse() {
        return mob.getTarget() != null && isHoldingBow();
    }

    protected boolean isHoldingBow() {
        return mob.isHolding(net.minecraft.world.item.Items.BOW);
    }

    @Override
    public boolean canContinueToUse() {
        return (canUse() || !mob.getNavigation().isDone()) && isHoldingBow();
    }

    @Override
    public void start() {
        mob.setAggressive(true);
    }

    @Override
    public void stop() {
        mob.setAggressive(false);
        seeTime = 0;
        attackTime = -1;
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
        double distance = mob.distanceToSqr(target.getX(), target.getY(), target.getZ());
        boolean visible = mob.getSensing().hasLineOfSight(target);
        seeTime = visible ? seeTime + 1 : 0;
        if (distance <= attackRadiusSqr && seeTime >= 20) {
            mob.getNavigation().stop();
        } else {
            mob.getNavigation().moveTo(target, speedModifier);
        }
        mob.getLookControl().setLookAt(target, 30.0F, 30.0F);
        if (--attackTime <= 0 && visible) {
            mob.performRangedAttack(target, net.minecraft.util.Mth.clamp((float) Math.sqrt(distance) / 15.0F, 0.1F, 1.0F));
            attackTime = attackIntervalMin;
        }
    }
}
