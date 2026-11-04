// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import net.minecraft.core.component.DataComponents;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.ai.util.LandRandomPos;
import net.minecraft.world.entity.monster.Monster;
import net.minecraft.world.item.component.KineticWeapon;
import net.minecraft.world.phys.Vec3;

public class SpearUseGoal<T extends Monster> extends Goal {
    private static final double kMaxFleeingTime = reducedTickDelay(100);
    private final T mob;
    private SpearUseState state;
    private final double speedModifierWhenCharging;
    private final double speedModifierWhenRepositioning;
    private final float approachDistanceSq;
    private final float targetInRangeRadiusSq;

    public SpearUseGoal(T mob, double speedModifierWhenCharging, double speedModifierWhenRepositioning, float approachDistance, float targetInRangeRadius) {
        this.mob = mob;
        this.speedModifierWhenCharging = speedModifierWhenCharging;
        this.speedModifierWhenRepositioning = speedModifierWhenRepositioning;
        this.approachDistanceSq = approachDistance * approachDistance;
        this.targetInRangeRadiusSq = targetInRangeRadius * targetInRangeRadius;
        setFlags(EnumSet.of(Goal.Flag.MOVE, Goal.Flag.LOOK));
    }

    private boolean ableToAttack() {
        return mob.getTarget() != null && mob.getMainHandItem().has(DataComponents.KINETIC_WEAPON);
    }

    private int weaponUseDuration() {
        KineticWeapon weapon = mob.getMainHandItem().get(DataComponents.KINETIC_WEAPON);
        return reducedTickDelay(weapon == null ? 0 : weapon.computeDamageUseDuration());
    }

    public boolean canUse() {
        return ableToAttack() && !mob.isUsingItem();
    }

    public boolean canContinueToUse() {
        return state != null && !state.done && ableToAttack();
    }

    public void start() {
        super.start();
        mob.setAggressive(true);
        state = new SpearUseState();
    }

    public void stop() {
        super.stop();
        mob.getNavigation().stop();
        mob.setAggressive(false);
        state = null;
        mob.stopUsingItem();
    }

    public void tick() {
        if (state == null) {
            return;
        }
        LivingEntity target = mob.getTarget();
        double distSqr = mob.distanceToSqr(target.getX(), target.getY(), target.getZ());
        float mountSpeed = mob.getRootVehicle() instanceof Mob vehicle ? vehicle.chargeSpeedModifier() : 1.0F;
        int mountBonus = mob.isPassenger() ? 2 : 0;
        mob.lookAt(target, 30.0F, 30.0F);
        mob.getLookControl().setLookAt(target, 30.0F, 30.0F);
        if (state.notEngagedYet()) {
            if (distSqr > approachDistanceSq) {
                mob.getNavigation().moveTo(target, mountSpeed * speedModifierWhenRepositioning);
                return;
            }
            state.startEngagement(weaponUseDuration());
            mob.startUsingItem(InteractionHand.MAIN_HAND);
        }
        if (state.tickAndCheckEngagement()) {
            mob.stopUsingItem();
            double distance = Math.sqrt(distSqr);
            state.awayPos = LandRandomPos.getPosAway(mob, Math.max(0.0, 9 + mountBonus - distance), Math.max(1.0, 11 + mountBonus - distance), 7, target.position());
            state.fleeingTime = 1;
        }
        if (state.tickAndCheckFleeing()) {
            return;
        }
        if (state.awayPos != null) {
            mob.getNavigation().moveTo(state.awayPos.x, state.awayPos.y, state.awayPos.z, mountSpeed * speedModifierWhenRepositioning);
            if (mob.getNavigation().isDone()) {
                if (state.fleeingTime > 0) {
                    state.done = true;
                    return;
                }
                state.awayPos = null;
            }
            return;
        }
        mob.getNavigation().moveTo(target, mountSpeed * speedModifierWhenCharging);
        if (distSqr < targetInRangeRadiusSq || mob.getNavigation().isDone()) {
            double distance = Math.sqrt(distSqr);
            state.awayPos = LandRandomPos.getPosAway(mob, 6 + mountBonus - distance, 7 + mountBonus - distance, 7, target.position());
        }
    }

    public static class SpearUseState {
        private int engageTime = -1;
        private int fleeingTime = -1;
        private Vec3 awayPos;
        private boolean done;

        public boolean notEngagedYet() {
            return engageTime < 0;
        }

        public void startEngagement(int spearDownTime) {
            engageTime = spearDownTime;
        }

        public boolean tickAndCheckEngagement() {
            return engageTime > 0 && --engageTime == 0;
        }

        public boolean tickAndCheckFleeing() {
            if (fleeingTime > 0 && ++fleeingTime > kMaxFleeingTime) {
                done = true;
                return true;
            }
            return false;
        }
    }
}
