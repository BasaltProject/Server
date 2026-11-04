// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import net.minecraft.util.TimeUtil;
import net.minecraft.util.valueproviders.UniformInt;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.monster.CrossbowAttackMob;
import net.minecraft.world.entity.monster.Monster;
import net.minecraft.world.entity.monster.RangedAttackMob;
import net.minecraft.world.item.Items;

public class RangedCrossbowAttackGoal<T extends Monster & RangedAttackMob & CrossbowAttackMob> extends Goal {
    public static final UniformInt PATHFINDING_DELAY_RANGE = TimeUtil.rangeOfSeconds(1, 2);
    private final T mob;
    private CrossbowState crossbowState = CrossbowState.UNCHARGED;
    private final double speedModifier;
    private final float attackRadiusSqr;
    private int seeTime;
    private int attackDelay;
    private int updatePathDelay;

    public RangedCrossbowAttackGoal(T mob, double speedModifier, float attackRadius) {
        this.mob = mob;
        this.speedModifier = speedModifier;
        this.attackRadiusSqr = attackRadius * attackRadius;
        setFlags(EnumSet.of(Goal.Flag.MOVE, Goal.Flag.LOOK));
    }

    @Override
    public boolean canUse() {
        return isValidTarget() && mob.isHolding(Items.CROSSBOW);
    }

    @Override
    public boolean canContinueToUse() {
        return isValidTarget() && (canUse() || !mob.getNavigation().isDone()) && mob.isHolding(Items.CROSSBOW);
    }

    private boolean isValidTarget() {
        return mob.getTarget() != null && mob.getTarget().isAlive();
    }

    @Override
    public void stop() {
        super.stop();
        mob.setAggressive(false);
        mob.setTarget(null);
        seeTime = 0;
        if (mob.isUsingItem()) {
            mob.stopUsingItem();
            mob.setChargingCrossbow(false);
        }
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
        boolean visible = mob.getSensing().hasLineOfSight(target);
        if (visible != seeTime > 0) {
            seeTime = 0;
        }
        seeTime += visible ? 1 : -1;
        boolean needsToMove = (mob.distanceToSqr(target) > attackRadiusSqr || seeTime < 5) && attackDelay == 0;
        if (needsToMove) {
            if (--updatePathDelay <= 0) {
                mob.getNavigation().moveTo(target, crossbowState == CrossbowState.UNCHARGED ? speedModifier : speedModifier * 0.5F);
                updatePathDelay = PATHFINDING_DELAY_RANGE.sample(mob.getRandom());
            }
        } else {
            updatePathDelay = 0;
            mob.getNavigation().stop();
        }
        mob.getLookControl().setLookAt(target, 30.0F, 30.0F);
        switch (crossbowState) {
            case UNCHARGED -> {
                if (!needsToMove) {
                    mob.startUsingItem(mob.getItemInHand(InteractionHand.MAIN_HAND).is(Items.CROSSBOW) ? InteractionHand.MAIN_HAND : InteractionHand.OFF_HAND);
                    crossbowState = CrossbowState.CHARGING;
                    mob.setChargingCrossbow(true);
                }
            }
            case CHARGING -> {
                if (!mob.isUsingItem()) {
                    crossbowState = CrossbowState.UNCHARGED;
                }
                if (mob.getTicksUsingItem() >= 25) {
                    mob.releaseUsingItem();
                    crossbowState = CrossbowState.CHARGED;
                    attackDelay = 20 + mob.getRandom().nextInt(20);
                    mob.setChargingCrossbow(false);
                }
            }
            case CHARGED -> {
                if (--attackDelay == 0) {
                    crossbowState = CrossbowState.READY_TO_ATTACK;
                }
            }
            case READY_TO_ATTACK -> {
                if (visible) {
                    mob.performRangedAttack(target, 1.0F);
                    crossbowState = CrossbowState.UNCHARGED;
                }
            }
        }
    }

    private enum CrossbowState {
        UNCHARGED, CHARGING, CHARGED, READY_TO_ATTACK
    }
}
