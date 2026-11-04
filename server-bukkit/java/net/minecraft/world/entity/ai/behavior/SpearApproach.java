// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Map;
import net.minecraft.core.component.DataComponents;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;

public class SpearApproach extends Behavior<PathfinderMob> {
    private final double speedModifierWhenRepositioning;
    private final float approachDistanceSq;

    public SpearApproach(double speedModifierWhenRepositioning, float approachDistance) {
        super(Map.of(MemoryModuleType.SPEAR_STATUS, MemoryStatus.VALUE_ABSENT));
        this.speedModifierWhenRepositioning = speedModifierWhenRepositioning;
        this.approachDistanceSq = approachDistance * approachDistance;
    }

    private boolean ableToAttack(PathfinderMob mob) {
        return getTarget(mob) != null && mob.getMainHandItem().has(DataComponents.KINETIC_WEAPON);
    }

    protected boolean checkExtraStartConditions(ServerLevel level, PathfinderMob body) {
        return ableToAttack(body) && !body.isUsingItem();
    }

    protected void start(ServerLevel level, PathfinderMob body, long timestamp) {
        body.setAggressive(true);
        body.getBrain().setMemory(MemoryModuleType.SPEAR_STATUS, SpearAttack.SpearStatus.APPROACH);
        super.start(level, body, timestamp);
    }

    private LivingEntity getTarget(PathfinderMob mob) {
        return mob.getBrain().getMemory(MemoryModuleType.ATTACK_TARGET).orElse(null);
    }

    protected boolean canStillUse(ServerLevel level, PathfinderMob body, long timestamp) {
        return ableToAttack(body) && farEnough(body);
    }

    private boolean farEnough(PathfinderMob mob) {
        LivingEntity target = getTarget(mob);
        return mob.distanceToSqr(target.getX(), target.getY(), target.getZ()) > approachDistanceSq;
    }

    protected void tick(ServerLevel level, PathfinderMob mob, long timestamp) {
        LivingEntity target = getTarget(mob);
        Entity mount = mob.getRootVehicle();
        float speedModifier = mount instanceof Mob vehicleMob ? vehicleMob.chargeSpeedModifier() : 1.0F;
        mob.getBrain().setMemory(MemoryModuleType.LOOK_TARGET, new EntityTracker(target, true));
        mob.getNavigation().moveTo(target, speedModifier * speedModifierWhenRepositioning);
    }

    protected void stop(ServerLevel level, PathfinderMob body, long timestamp) {
        body.getNavigation().stop();
        body.getBrain().setMemory(MemoryModuleType.SPEAR_STATUS, SpearAttack.SpearStatus.CHARGING);
    }

    protected boolean timedOut(long timestamp) {
        return false;
    }
}
