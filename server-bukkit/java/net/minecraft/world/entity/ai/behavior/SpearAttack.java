// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Map;
import java.util.Optional;
import net.minecraft.core.component.DataComponents;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.entity.ai.util.LandRandomPos;
import net.minecraft.world.item.component.KineticWeapon;
import net.minecraft.world.phys.Vec3;

public class SpearAttack extends Behavior<PathfinderMob> {
    public static final int MIN_REPOSITION_DISTANCE = 6;
    public static final int MAX_REPOSITION_DISTANCE = 7;
    private final double speedModifierWhenCharging;
    private final double speedModifierWhenRepositioning;
    private final float targetInRangeRadiusSq;

    public SpearAttack(double speedModifierWhenCharging, double speedModifierWhenRepositioning, float targetInRangeRadius) {
        super(Map.of(MemoryModuleType.SPEAR_STATUS, MemoryStatus.VALUE_PRESENT));
        this.speedModifierWhenCharging = speedModifierWhenCharging;
        this.speedModifierWhenRepositioning = speedModifierWhenRepositioning;
        this.targetInRangeRadiusSq = targetInRangeRadius * targetInRangeRadius;
    }

    private LivingEntity getTarget(PathfinderMob mob) {
        return mob.getBrain().getMemory(MemoryModuleType.ATTACK_TARGET).orElse(null);
    }

    private boolean ableToAttack(PathfinderMob mob) {
        return getTarget(mob) != null && mob.getMainHandItem().has(DataComponents.KINETIC_WEAPON);
    }

    private int getKineticWeaponUseDuration(PathfinderMob mob) {
        return Optional.ofNullable((KineticWeapon) mob.getMainHandItem().get(DataComponents.KINETIC_WEAPON)).map(KineticWeapon::computeDamageUseDuration).orElse(0);
    }

    protected boolean checkExtraStartConditions(ServerLevel level, PathfinderMob body) {
        return body.getBrain().getMemory(MemoryModuleType.SPEAR_STATUS).orElse(SpearStatus.APPROACH) == SpearStatus.CHARGING && ableToAttack(body) && !body.isUsingItem();
    }

    protected void start(ServerLevel level, PathfinderMob body, long timestamp) {
        body.setAggressive(true);
        body.getBrain().setMemory(MemoryModuleType.SPEAR_ENGAGE_TIME, getKineticWeaponUseDuration(body));
        body.getBrain().eraseMemory(MemoryModuleType.SPEAR_CHARGE_POSITION);
        body.startUsingItem(InteractionHand.MAIN_HAND);
        super.start(level, body, timestamp);
    }

    protected boolean canStillUse(ServerLevel level, PathfinderMob body, long timestamp) {
        return body.getBrain().getMemory(MemoryModuleType.SPEAR_ENGAGE_TIME).orElse(0) > 0 && ableToAttack(body);
    }

    protected void tick(ServerLevel level, PathfinderMob mob, long timestamp) {
        LivingEntity quarry = getTarget(mob);
        Brain<?> mind = mob.getBrain();
        float chargeScale = mob.getRootVehicle() instanceof Mob steed ? steed.chargeSpeedModifier() : 1.0F;
        mind.setMemory(MemoryModuleType.LOOK_TARGET, new EntityTracker(quarry, true));
        int remaining = mind.getMemory(MemoryModuleType.SPEAR_ENGAGE_TIME).orElse(0);
        mind.setMemory(MemoryModuleType.SPEAR_ENGAGE_TIME, remaining - 1);
        Optional<Vec3> retreatSpot = mind.getMemory(MemoryModuleType.SPEAR_CHARGE_POSITION);
        if (retreatSpot.isPresent()) {
            Vec3 spot = retreatSpot.get();
            mob.getNavigation().moveTo(spot.x, spot.y, spot.z, chargeScale * speedModifierWhenRepositioning);
            if (mob.getNavigation().isDone()) {
                mind.eraseMemory(MemoryModuleType.SPEAR_CHARGE_POSITION);
            }
            return;
        }
        mob.getNavigation().moveTo(quarry, chargeScale * speedModifierWhenCharging);
        double gapSq = mob.distanceToSqr(quarry.getX(), quarry.getY(), quarry.getZ());
        if (gapSq < targetInRangeRadiusSq || mob.getNavigation().isDone()) {
            int seat = mob.isPassenger() ? 2 : 0;
            double gap = Math.sqrt(gapSq);
            Vec3 away = LandRandomPos.getPosAway(mob, MIN_REPOSITION_DISTANCE + seat - gap, MAX_REPOSITION_DISTANCE + seat - gap, 7, quarry.position());
            mind.setMemory(MemoryModuleType.SPEAR_CHARGE_POSITION, away);
        }
    }

    protected void stop(ServerLevel level, PathfinderMob body, long timestamp) {
        body.stopUsingItem();
        body.getNavigation().stop();
        Brain<?> mind = body.getBrain();
        mind.eraseMemory(MemoryModuleType.SPEAR_ENGAGE_TIME);
        mind.eraseMemory(MemoryModuleType.SPEAR_CHARGE_POSITION);
        mind.setMemory(MemoryModuleType.SPEAR_STATUS, SpearStatus.RETREAT);
    }

    protected boolean timedOut(long timestamp) {
        return false;
    }

    public static enum SpearStatus {
        APPROACH, CHARGING, RETREAT;
    }
}
